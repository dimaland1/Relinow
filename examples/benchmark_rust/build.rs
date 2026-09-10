use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

fn find_compile_commands() -> Option<PathBuf> {
    let out_dir = PathBuf::from(env::var("OUT_DIR").ok()?);
    let build_root = out_dir.parent()?.parent()?;
    for entry in fs::read_dir(build_root).ok()? {
        if let Ok(entry) = entry {
            let path = entry.path();
            if path.is_dir() && path.file_name().map_or(false, |n| n.to_string_lossy().starts_with("esp-idf-sys-")) {
                let candidate = path.join("out/build/compile_commands.json");
                if candidate.exists() {
                    return Some(candidate);
                }
            }
        }
    }
    None
}

fn extract_includes(compile_commands_path: &Path) -> Vec<PathBuf> {
    let content = fs::read_to_string(compile_commands_path).unwrap_or_default();
    let mut includes = BTreeSet::new();
    for part in content.split("-I") {
        if let Some(token) = part.split_whitespace().next() {
            let clean = token.trim_matches(|c| c == '"' || c == '\\' || c == ',' || c == '}' || c == ']');
            if !clean.is_empty() && (clean.starts_with('/') || clean.contains(':')) {
                includes.insert(PathBuf::from(clean));
            }
        }
    }
    includes.into_iter().collect()
}

fn find_toolchain_includes() -> Vec<PathBuf> {
    let mut dirs = Vec::new();
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap_or_default());
    let tools_dir = manifest_dir.join(".embuild/espressif/tools/xtensa-esp-elf");
    if tools_dir.exists() {
        if let Ok(entries) = fs::read_dir(&tools_dir) {
            for entry in entries.flatten() {
                let base = entry.path().join("xtensa-esp-elf");
                let inc1 = base.join("xtensa-esp-elf/include");
                if inc1.join("stdio.h").exists() {
                    dirs.push(inc1);
                }
                let gcc_dir = base.join("lib/gcc/xtensa-esp-elf");
                if let Ok(versions) = fs::read_dir(&gcc_dir) {
                    for ver in versions.flatten() {
                        let inc2 = ver.path().join("include");
                        if inc2.join("stddef.h").exists() {
                            dirs.push(inc2);
                        }
                    }
                }
            }
        }
    }
    dirs
}

fn find_compiler() -> Option<PathBuf> {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap_or_default());
    let tools_dir = manifest_dir.join(".embuild/espressif/tools/xtensa-esp-elf");
    if tools_dir.exists() {
        if let Ok(entries) = fs::read_dir(&tools_dir) {
            for entry in entries.flatten() {
                let gcc = entry.path().join("xtensa-esp-elf/bin/xtensa-esp32-elf-gcc");
                if gcc.exists() {
                    return Some(gcc);
                }
            }
        }
    }
    None
}

fn main() {
    embuild::espidf::sysenv::output();

    let compile_commands = find_compile_commands()
        .expect("Could not find compile_commands.json in esp-idf-sys output");
    let includes = extract_includes(&compile_commands);
    let toolchain_includes = find_toolchain_includes();

    println!("cargo:warning=Found {} ESP-IDF include directories", includes.len());
    println!("cargo:warning=Found {} Toolchain include directories", toolchain_includes.len());

    let mut build = cc::Build::new();
    if let Some(compiler) = find_compiler() {
        println!("cargo:warning=Using compiler: {}", compiler.display());
        build.compiler(&compiler);
        let ar = compiler.with_file_name("xtensa-esp32-elf-gcc-ar");
        if ar.exists() {
            build.archiver(ar);
        }
    }
    build
        .flag("-mlongcalls")
        .file("../../core/src/relinow_espnow.c")
        .file("../../core/src/relinow_packet.c")
        .file("../../core/src/relinow_state.c")
        .file("../../core/src/relinow_reliable.c")
        .include("../../core/include");

    for inc in &includes {
        build.include(inc);
    }
    build.compile("relinow_core");

    let mut builder = bindgen::builder()
        .header("../../core/include/relinow_espnow.h")
        .use_core()
        .layout_tests(false)
        .allowlist_function("relinow_.*")
        .allowlist_type("relinow_.*")
        .allowlist_var("RELINOW_.*")
        .clang_arg("-DESP_PLATFORM")
        .clang_arg("-I../../core/include");

    for inc in &toolchain_includes {
        builder = builder.clang_arg(format!("-isystem{}", inc.display()));
    }

    for inc in &includes {
        builder = builder.clang_arg(format!("-I{}", inc.display()));
    }

    let bindings = builder
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}

