#!/usr/bin/env python3
"""Build/run portable and application tests, with temporary binaries only."""
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parent
compiler=os.environ.get('CXX','c++')
cases=('boot','pullup','navigation','ordering','display','sequence_ui','encoder',
       'serial_guard','gpio_reapply','uart_full','all_pages','scroll_sensitivity',
       'manual_hold','manual_dry_tap','manual_boot_and_exit','manual_loop_release',
       'settings_migration','settings_reboot','settings_invalid_ui',
       'timer_blocked_ui','timer_dry','timer_late_irq','timer_stop_stale',
       'timer_init_failure','timer_arm_failure','timer_read_failure',
       'timer_maximum_wrap','timer_late_loop','timer_late_start','timer_no_flash',
       'theme_preview','theme_invalid','timing_navigation',
       'lock_controls','lock_menu_stop','lock_phases','lock_pause',
       'lock_gesture_short','lock_gesture_long','lock_gesture_debounce','lock_failure',
       'subsecond','camera_mode','capture_migration','capture_reboot',
       'manual_fast','manual_spi','panel_calibration','timing_series_stress','serial_recovery',
       'timer_arm_overdue','manual_press_display_order','manual_release_display_order','stop_display_order')
cases+=('fraction_editor','camera_speed_reminder','camera_speed_invalid','custom_exposure_retained')
cases+=('timer_cache_safe', 'timer_cache_legacy_control', 'timer_global_mask', 'timer_clock_after_start', 'timer_intr_handle_failure', 'timer_iram_mark_failure')
cases+=('button_redraw_click', 'button_redraw_ordering', 'button_boot_held', 'button_serial_state')
cases+=('reset_report',)
cases+=('timer_prepare_low','timer_setup_budget','timer_lost_irq','timer_timeout_render','render_defers_save','render_defers_rotation')
cases+=('display_no_framebuffer',)
cases+=('timer_setup_retry',)
with tempfile.TemporaryDirectory(prefix='r7-tests-') as tmp:
    for name in ('input','sequence','controls','exposure'):
        binary=str(Path(tmp)/name)
        subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-pedantic',str(root/(name+'_test.cpp')),'-o',binary],check=True)
        subprocess.run([binary],check=True)
    binary=str(Path(tmp)/'app')
    subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-pedantic',
                    '-fsanitize=address,undefined','-I'+str(root/'host'),str(root/'app_test.cpp'),str(root.parent/'PulseTimer.cpp'),'-o',binary],check=True)
    for case in cases:subprocess.run([binary,case],check=True,timeout=15)
