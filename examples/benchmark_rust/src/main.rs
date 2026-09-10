#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

use esp_idf_sys as _; // If needed to pull in link sections
use esp_idf_hal::peripherals::Peripherals;
use esp_idf_svc::wifi::EspWifi;
use esp_idf_svc::eventloop::EspSystemEventLoop;
use esp_idf_svc::nvs::EspDefaultNvsPartition;
use log::{info, warn, error};
use std::time::Duration;

const BENCHMARK_CHANNEL: u8 = 10;
const PACKET_COUNT: u16 = 1000;
const PAYLOAD_SIZE: u16 = 241;

// Global node and discovery state
static mut NODE: relinow_espnow_node_t = unsafe { std::mem::zeroed() };
static mut MY_MAC: [u8; 6] = [0; 6];
static mut PEER_MAC: [u8; 6] = [0; 6];
static mut PEER_DISCOVERED: bool = false;
static mut AM_I_SENDER: bool = false;

fn get_my_mac() -> [u8; 6] {
    let mut mac = [0u8; 6];
    unsafe {
        esp_idf_sys::esp_read_mac(mac.as_mut_ptr(), esp_idf_sys::esp_mac_type_t_ESP_MAC_WIFI_STA);
    }
    mac
}

unsafe extern "C" fn app_recv_cb(esp_now_info: *const esp_idf_sys::esp_now_recv_info_t, data: *const u8, data_len: i32) {
    if esp_now_info.is_null() || data.is_null() || data_len <= 0 {
        return;
    }
    let src_addr = (*esp_now_info).src_addr;
    if !PEER_DISCOVERED {
        let bcast = [0xFFu8; 6];
        if std::slice::from_raw_parts(src_addr, 6) != bcast {
            std::ptr::copy_nonoverlapping(src_addr, PEER_MAC.as_mut_ptr(), 6);
            PEER_DISCOVERED = true;

            // Determine role: Lowest MAC address is Sender
            AM_I_SENDER = std::slice::from_raw_parts(MY_MAC.as_ptr(), 6) < std::slice::from_raw_parts(PEER_MAC.as_ptr(), 6);

            warn!("Peer Discovered: {:02X?}", PEER_MAC);
            warn!("My Role: {}", if AM_I_SENDER { "SENDER (Initiator)" } else { "RECEIVER (Responder)" });
        }
    }

    let now = (esp_idf_sys::esp_timer_get_time() / 1000) as u32;
    relinow_espnow_on_receive(&mut NODE, src_addr, data, data_len as u16, now);
}

fn main() {
    esp_idf_sys::link_patches();
    esp_idf_svc::log::EspLogger::initialize_default();

    std::thread::Builder::new()
        .stack_size(24 * 1024)
        .spawn(run_benchmark)
        .unwrap()
        .join()
        .unwrap();
}

fn run_benchmark() {
    info!("Starting Benchmark in Rust!");

    let peripherals = Peripherals::take().unwrap();
    let sysloop = EspSystemEventLoop::take().unwrap();
    let nvs = EspDefaultNvsPartition::take().unwrap();

    let mut wifi = EspWifi::new(
        peripherals.modem,
        sysloop.clone(),
        Some(nvs),
    ).unwrap();

    wifi.set_configuration(&esp_idf_svc::wifi::Configuration::Client(Default::default())).unwrap();
    wifi.start().unwrap();

    let my_mac = get_my_mac();
    unsafe {
        MY_MAC.copy_from_slice(&my_mac);
    }
    warn!("My MAC Address: {:02X?}", my_mac);

    unsafe {
        esp_idf_sys::esp_wifi_set_channel(1, esp_idf_sys::wifi_second_chan_t_WIFI_SECOND_CHAN_NONE);
        esp_idf_sys::esp_now_init();
        esp_idf_sys::esp_now_register_recv_cb(Some(app_recv_cb));
    }

    let mut rcfg: relinow_espnow_config_t = unsafe { std::mem::zeroed() };
    unsafe { relinow_espnow_default_config(&mut rcfg) };

    unsafe {
        if relinow_espnow_init(&mut NODE, &rcfg) != 0 {
            error!("Failed to init relinow");
            return;
        }
    }

    // Register broadcast peer in ESP-NOW
    let bcast_mac = [0xFFu8; 6];
    let mut bcast_peer: esp_idf_sys::esp_now_peer_info_t = unsafe { std::mem::zeroed() };
    bcast_peer.peer_addr.copy_from_slice(&bcast_mac);
    bcast_peer.channel = 1;
    bcast_peer.ifidx = esp_idf_sys::wifi_interface_t_WIFI_IF_STA;
    unsafe {
        esp_idf_sys::esp_now_add_peer(&bcast_peer);

        let mut bcast_idx = 0u8;
        relinow_state_add_peer(&mut NODE.state, bcast_mac.as_ptr(), &mut bcast_idx);
        relinow_state_open_channel(&mut NODE.state, bcast_idx, 1, RELINOW_MODE_UNRELIABLE as u8, 5);
        NODE.peer_index = bcast_idx;
        NODE.peer_mac.copy_from_slice(&bcast_mac);
    }

    info!("Starting Auto-Discovery Broadcast...");

    let payload = b"DISCOVER";
    loop {
        unsafe {
            if PEER_DISCOVERED { break; }
            relinow_espnow_send_unreliable(&mut NODE, 1, payload.as_ptr(), payload.len() as u16);
        }
        std::thread::sleep(Duration::from_millis(500));
    }

    info!("Peer found! Registering...");

    unsafe {
        let mut peer_info: esp_idf_sys::esp_now_peer_info_t = std::mem::zeroed();
        peer_info.peer_addr.copy_from_slice(&PEER_MAC);
        peer_info.channel = 1;
        peer_info.ifidx = esp_idf_sys::wifi_interface_t_WIFI_IF_STA;
        esp_idf_sys::esp_now_add_peer(&peer_info);

        let mut p_idx = 0u8;
        relinow_state_add_peer(&mut NODE.state, PEER_MAC.as_ptr(), &mut p_idx);
        NODE.peer_index = p_idx;
        NODE.peer_mac.copy_from_slice(&PEER_MAC);

        let mut cfg: relinow_reliable_config_t = std::mem::zeroed();
        relinow_reliable_default_config(&mut cfg);
        cfg.initial_rtt_ms = 50;

        relinow_state_set_reliable_config(&mut NODE.state, p_idx, BENCHMARK_CHANNEL, &cfg);
        relinow_state_open_channel(&mut NODE.state, p_idx, BENCHMARK_CHANNEL, RELINOW_MODE_RELIABLE as u8, 5);
    }

    info!("Waiting 3 seconds to synchronize...");
    std::thread::sleep(Duration::from_secs(3));

    let am_i_sender = unsafe { AM_I_SENDER };
    info!("Role: {}", if am_i_sender { "SENDER" } else { "RESPONDER" });

    if am_i_sender {
        info!("--- STARTING BENCHMARK ---");
        let payload = [0xABu8; PAYLOAD_SIZE as usize];
        let mut packets_sent = 0;

        let start_time = unsafe { esp_idf_sys::esp_timer_get_time() };

        loop {
            let now = unsafe { esp_idf_sys::esp_timer_get_time() } as u32 / 1000;
            unsafe { relinow_espnow_poll(&mut NODE, now); }

            if packets_sent < PACKET_COUNT {
                unsafe {
                    let err = relinow_espnow_send_reliable(&mut NODE, BENCHMARK_CHANNEL, payload.as_ptr(), PAYLOAD_SIZE, now);
                    if err == 0 {
                        packets_sent += 1;
                        if packets_sent % 100 == 0 {
                            info!("Sent {}/{}...", packets_sent, PACKET_COUNT);
                        }
                    }
                }
            }

            let mut inflight = 0;
            unsafe {
                let p_idx = NODE.peer_index;
                let tx_queue = &NODE.state.peers[p_idx as usize].channels[BENCHMARK_CHANNEL as usize].tx_queue;
                for slot in tx_queue.iter() {
                    if slot.used != 0 {
                        inflight = 1;
                        break;
                    }
                }
            }

            if packets_sent == PACKET_COUNT && inflight == 0 {
                break;
            }

            std::thread::sleep(Duration::from_millis(1));
        }

        let end_time = unsafe { esp_idf_sys::esp_timer_get_time() };
        let duration_s = (end_time - start_time) as f32 / 1000000.0;
        let total_bytes = PACKET_COUNT as f32 * PAYLOAD_SIZE as f32;
        let kbps = (total_bytes / 1024.0) / duration_s;

        let mut rtt = 0;
        unsafe {
            relinow_state_reliable_get_rtt_ms(&NODE.state, NODE.peer_index, BENCHMARK_CHANNEL, &mut rtt);
        }

        info!("========================================");
        info!(" BENCHMARK COMPLETE");
        info!("========================================");
        info!("Packets: {}", PACKET_COUNT);
        info!("Payload: {} bytes/packet", PAYLOAD_SIZE);
        info!("Time   : {:.2} seconds", duration_s);
        info!("Speed  : {:.2} KB/s ({:.2} Mbps)", kbps, (kbps * 8.0) / 1024.0);
        info!("Avg RTT: {} ms", rtt);
        info!("========================================");

        loop {
            std::thread::sleep(Duration::from_secs(1));
        }
    } else {
        info!("--- RESPONDER READY ---");
        loop {
            let now = unsafe { esp_idf_sys::esp_timer_get_time() } as u32 / 1000;
            unsafe { relinow_espnow_poll(&mut NODE, now); }
            std::thread::sleep(Duration::from_millis(1));
        }
    }
}

