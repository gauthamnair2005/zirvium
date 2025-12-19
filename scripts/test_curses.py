#!/usr/bin/env python3
import curses
import time

def test_curses(stdscr):
    # Clear and refresh
    stdscr.clear()
    stdscr.refresh()
    
    # Get terminal size
    height, width = stdscr.getmaxyx()
    
    # Setup colors
    curses.start_color()
    curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)
    curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLUE)
    
    # Draw title
    stdscr.attron(curses.color_pair(1))
    stdscr.addstr(0, 0, " " * width)
    stdscr.addstr(0, 2, "Test Curses Application")
    stdscr.attroff(curses.color_pair(1))
    
    # Draw content
    stdscr.attron(curses.color_pair(2))
    for i in range(1, height - 1):
        stdscr.addstr(i, 0, " " * width)
    stdscr.attroff(curses.color_pair(2))
    
    # Draw some menu items
    menu_items = ["Option 1", "Option 2", "Option 3", "Option 4", "Option 5"]
    for i, item in enumerate(menu_items):
        stdscr.addstr(i + 2, 4, f"[*] {item}")
    
    # Draw status bar
    status = "Press 'q' to quit"
    stdscr.addstr(height - 1, 2, status)
    
    # Refresh and wait
    stdscr.refresh()
    
    # Wait for key
    while True:
        key = stdscr.getch()
        if key == ord('q'):
            break

if __name__ == "__main__":
    try:
        curses.wrapper(test_curses)
        print("Test completed successfully!")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
