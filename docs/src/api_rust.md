# Rust API & Embedded Integration

ReliNow provides modern Rust support via two complementary approaches:
1. **`relinow` Crate (`rust/relinow/`)**: A `#![no_std]` native Rust implementation designed for bare-metal targets (`esp-hal`, `cortex-m`, etc.).
2. **`relinow-sys` / FFI Integration (`examples/benchmark_rust/`)**: Zero-overhead FFI bindings linking the production-hardened C99 core engine directly into ESP-IDF Rust applications.

---

## 1. Safety & Architecture

Embedded Rust applications benefit from:
- **Zero dynamic memory allocation**: All state structs and queues are statically or stack allocated.
- **Type-safe Mode Selection**: Transmission modes are represented by strong enums rather than raw integers.
- **Strict Lifetime Guarantees**: Buffers passed to send methods are guaranteed to be read safely without dangling pointer risks.

---

## 2. ESP-IDF Rust Integration Pattern

When deploying on ESP32 targets with `esp-idf-sys` / `esp-idf-svc`, standard POSIX threads (`pthread`) are available through Rust's `std::thread`.

### Critical Sizing Guideline: Stack Space
Under ESP-IDF, the default FreeRTOS `main_task` stack is configured to **3.5 KB**, which is insufficient for the Rust Wi-Fi driver and TLS/eventloop runtime structures. Always run your ReliNow loop in a dedicated thread with an expanded stack:

```rust
std::thread::Builder::new()
    .stack_size(24 * 1024) // 24 KB stack
    .name("relinow_main".to_string())
    .spawn(run_app)
    .unwrap()
    .join()
    .unwrap();
```

---

## 3. Minimal Rust Working Example

Below is a complete, real-world example of an ESP-NOW node transmitting and receiving reliable packets in Rust:

```rust
use esp_idf_hal::peripherals::Peripherals;
use esp_idf_svc::eventloop::EspSystemEventLoop;
use esp_idf_svc::nvs::EspDefaultNvsPartition;
use esp_idf_svc::wifi::{Configuration, EspWifi};
use log::{info, warn};
use std::time::Duration;

// Include generated FFI bindings or relinow crate
use relinow_sys::*;

static mut NODE: relinow_node_t = unsafe { std::mem::zeroed() };

unsafe extern "C" fn on_receive(
    esp_now_info: *const esp_idf_sys::esp_now_recv_info_t,
    data: *const u8,
    data_len: i32,
) {
    if !esp_now_info.is_null() && !data.is_null() && data_len > 0 {
        let now_ms = (esp_idf_sys::esp_timer_get_time() / 1000) as u32;
        relinow_on_receive(&mut NODE, (*esp_now_info).src_addr, data, data_len as u16, now_ms);
    }
}

fn main() {
    esp_idf_sys::link_patches();
    esp_idf_svc::log::EspLogger::initialize_default();

    std::thread::Builder::new()
        .stack_size(24 * 1024)
        .spawn(|| {
            let peripherals = Peripherals::take().unwrap();
            let sysloop = EspSystemEventLoop::take().unwrap();
            let nvs = EspDefaultNvsPartition::take().unwrap();

            let mut wifi = EspWifi::new(peripherals.modem, sysloop, Some(nvs)).unwrap();
            wifi.set_configuration(&Configuration::Client(Default::default())).unwrap();
            wifi.start().unwrap();

            unsafe {
                esp_idf_sys::esp_wifi_set_channel(1, esp_idf_sys::wifi_second_chan_t_WIFI_SECOND_CHAN_NONE);
                esp_idf_sys::esp_now_init();
                esp_idf_sys::esp_now_register_recv_cb(Some(on_receive));
            }

            info!("ReliNow node initialized in Rust!");

            loop {
                let now_ms = (unsafe { esp_idf_sys::esp_timer_get_time() } / 1000) as u32;
                unsafe { relinow_poll(&mut NODE, now_ms); }
                std::thread::sleep(Duration::from_millis(5));
            }
        })
        .unwrap()
        .join()
        .unwrap();
}
```
