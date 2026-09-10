fn main() {
    let core_src_dir = "../../core/src";
    let core_inc_dir = "../../core/include";

    cc::Build::new()
        .include(core_inc_dir)
        .file(format!("{}/relinow_packet.c", core_src_dir))
        .file(format!("{}/relinow_reliable.c", core_src_dir))
        .file(format!("{}/relinow_state.c", core_src_dir))
        .compile("relinow");

    println!("cargo:rerun-if-changed={}/relinow_packet.h", core_inc_dir);
    println!("cargo:rerun-if-changed={}/relinow_reliable.h", core_inc_dir);
    println!("cargo:rerun-if-changed={}/relinow_state.h", core_inc_dir);
}
