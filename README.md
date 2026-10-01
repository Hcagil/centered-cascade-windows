# Centered Cascade Windows

A [Windhawk](https://windhawk.net/) mod for Windows 11 that arranges ordinary app windows in a centered diagonal cascade. Windows follow their taskbar icons from left to right. Drag a taskbar icon to reorder its open windows; the layout updates within about two seconds.

## Behavior

- New windows join the cascade; windows under the same taskbar icon stay together in opening order.
- Closing or minimizing a window recenters the remaining windows. Restoring it adds it back.
- The cascade is centered as a group. Each monitor is arranged independently.
- Each Windows virtual desktop has its own cascade. Windows on another desktop do not count toward the active desktop's positions.
- Maximized windows, dialogs, tool windows, and Nahimic are excluded. File Explorer is included.
- Window size and horizontal/vertical steps are configurable in Windhawk. Defaults are 2500 × 1550 px and 40 × 30 px, scaled from a 3000 × 2000 display to other monitor sizes. The steps shrink when needed to keep the cascade on screen.

## Install

1. Install [Windhawk](https://windhawk.net/) on Windows 11.
2. In Windhawk, create a local mod, paste the contents of [`cascade-windows.wh.cpp`](cascade-windows.wh.cpp), then compile and enable it.
3. Adjust **Window width**, **Window height**, and the two **Shift** settings in the mod's settings page.

Disable the mod in Windhawk to stop automatic positioning.

## Notes

Taskbar order is read from the Windows 11 taskbar accessibility tree. Apps without a matching AppUserModelID are placed after matched apps, retaining their previous order. The mod targets 64-bit `explorer.exe` and has been tested on one Windows 11 laptop with a 3000 × 2000 display and two virtual desktops.
