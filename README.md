# Centered Cascade Windows

A [Windhawk](https://windhawk.net/) mod for Windows 11 that arranges ordinary app windows in a centered diagonal cascade. Windows follow their taskbar icons from left to right. Drag a taskbar icon to reorder its open windows; the layout updates within about two seconds.

## Behavior

- New windows join the cascade; windows under the same taskbar icon stay together in opening order.
- Closing or minimizing a window recenters the remaining windows. Restoring it adds it back.
- The cascade is centered as a group. Each monitor is arranged independently.
- Each Windows virtual desktop has its own cascade. Windows on another desktop do not count toward the active desktop's positions.
- Maximized windows, dialogs, tool windows, and Nahimic are excluded. File Explorer is included.
- Window width, height, ratio, and horizontal/vertical steps are configurable. **Screen ratio** defaults to **Current monitor** (3:2 on a 3000 × 2000 display) and can be set to 3:2, 16:10, or 16:9. The starting width is 2500 px and the steps default to 40 × 30 px. Dimensions scale to other monitors, and the steps shrink when needed to keep the cascade on screen.

## Install

1. Install [Windhawk](https://windhawk.net/) on Windows 11.
2. In Windhawk, create a local mod, paste the contents of [`cascade-windows.wh.cpp`](cascade-windows.wh.cpp), then compile and enable it.
3. Use Windhawk settings for the two **Shift** values. To edit window dimensions, double-click [`Open Window Size Editor.cmd`](Open%20Window%20Size%20Editor.cmd) from this repository folder. The editor requires one administrator prompt to save the values into Windhawk.

Disable the mod in Windhawk to stop automatic positioning.

## Notes

Taskbar order is read from the Windows 11 taskbar accessibility tree. Apps without a matching AppUserModelID are placed after matched apps, retaining their previous order. The mod targets 64-bit `explorer.exe` and has been tested on one Windows 11 laptop with a 3000 × 2000 display and two virtual desktops.

The size editor updates the paired dimension while you type or change the ratio. Click **Save to Windhawk** to store both pixel values and refresh the layout. Windhawk 1.7.3's built-in settings page cannot update one field from another while typing, so use the separate editor when changing size or ratio. The editor uses Windows PowerShell and Windows Forms already included with Windows; it installs no additional software.
