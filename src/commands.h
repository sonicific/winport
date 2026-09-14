#pragma once

namespace sonic79::command {

constexpr int refresh = 40100;
constexpr int restart_elevated = 40101;
constexpr int create_restore_point = 40102;
constexpr int exit = 40104;

constexpr int select_all = 40110;
constexpr int remove_selected = 40111;
constexpr int properties = 40112;
constexpr int copy = 40113;

constexpr int show_enumerator = 40120;
constexpr int show_service = 40121;
constexpr int show_com_port = 40122;
constexpr int show_device_id = 40123;
constexpr int show_root = 40124;
constexpr int show_swd = 40125;
constexpr int show_sw = 40126;
constexpr int dark_mode = 40127;
constexpr int always_on_top = 40128;

constexpr int bluetooth = 40130;
constexpr int device_manager = 40131;
constexpr int disk_management = 40132;
constexpr int event_viewer = 40133;
constexpr int network_adapters = 40134;

constexpr int about = 40140;

}  // namespace sonic79::command
