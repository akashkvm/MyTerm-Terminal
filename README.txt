=====================
MyTerm - Custom Terminal Emulator
=====================

1. Compilation Instructions
---------------------------
To compile MyTerm, run:

make clean
make

This will generate the executable file `myterm`.

Prerequisites:
- X11 development libraries must be installed
- On Ubuntu/Debian: sudo apt install libx11-dev
- On CentOS/RHEL: sudo yum install libX11-devel

2. Running MyTerm
-----------------
To launch the terminal:

./myterm

Ensure that X11 is installed and configured properly on your system.
MyTerm will open in a new X11 window with a terminal interface.

3. Testing Commands and Features
--------------------------------

3.1 Basic Commands
------------------
Run external commands:
```
user@myterm> ls -l
user@myterm> date
user@myterm> whoami
user@myterm> pwd
```

3.2 Built-in Commands
---------------------
Change directory:
```
user@myterm> cd /home
user@myterm> cd ~
user@myterm> cd ..
```

Print working directory:
```
user@myterm> pwd
```

Echo text:
```
user@myterm> echo Hello World
user@myterm> echo "This is a test"
```

Exit MyTerm:
```
user@myterm> exit
```

3.3 Input/Output Redirection
----------------------------
Input redirection:
```
user@myterm> cat < input.txt
user@myterm> sort < data.txt
```

Output redirection:
```
user@myterm> ls -l > filelist.txt
user@myterm> echo "Hello" > greeting.txt
```

Combined input/output redirection:
```
user@myterm> sort < input.txt > sorted.txt
user@myterm> grep "test" < data.txt > results.txt
```

3.4 Pipe Support
----------------
Basic pipes:
```
user@myterm> ls | wc -l
user@myterm> cat file.txt | sort
user@myterm> ps aux | grep bash
```

Multiple pipes:
```
user@myterm> cat file.txt | sort | uniq | wc -l
user@myterm> ls *.txt | xargs cat | grep "pattern"
```

3.5 MultiWatch Command
----------------------
Basic multiWatch:
```
user@myterm> multiWatch ["date", "who", "ls"]
```

File operations:
```
user@myterm> multiWatch ["ls *.txt", "cat file.txt", "wc -l file.txt"]
```

System monitoring:
```
user@myterm> multiWatch ["ps aux", "df -h", "free -h"]
```

Network operations:
```
user@myterm> multiWatch ["ping -c 3 google.com", "curl -s ifconfig.me"]
```

3.6 Line Navigation
-------------------
- Ctrl+A → Move cursor to start of line
- Ctrl+E → Move cursor to end of line

Test by typing a long command and using these shortcuts to navigate.

3.7 Signal Handling
-------------------
Interrupt command (Ctrl+C):
```
user@myterm> sleep 10
# Press Ctrl+C to stop the command
```

Background command (Ctrl+Z):
```
user@myterm> sleep 10
# Press Ctrl+Z to move to background
# Shows: [PID] Stopped and moved to background.
```

3.8 Searchable History
----------------------
View command history:
```
user@myterm> history
```

Search history (Ctrl+R):
```
# Press Ctrl+R to activate search
# Shows search prompt and functionality
```

3.9 Copy-Paste Functionality
----------------------------
- Select text with mouse
- Ctrl+B to copy selected text
- Ctrl+V to paste from clipboard

3.10 Multiline Input
--------------------
Use backslash (\) at end of line to continue on next line:
```
user@myterm> echo "This is a very long command that \
> spans multiple lines"
```

4. Advanced Features
--------------------

4.1 Tab Management
------------------
- Multiple tabs supported (up to 5)
- Tab switching with mouse clicks
- Each tab maintains independent shell process

4.2 Unicode Support
-------------------
- Full Unicode character support
- Proper text rendering for international characters
- Locale-aware input handling

4.3 Text Selection
------------------
- Mouse-based text selection
- Visual selection highlighting
- Copy selected text to clipboard

4.4 History Persistence
-----------------------
- Commands saved to .myterm_history file
- History persists across MyTerm sessions
- Automatic trimming to 10,000 entries
- Last 1,000 commands displayed with 'history' command

5. File Structure
-----------------
Essential files:
- main.c              : Main application entry point
- x11_gui.c           : X11 GUI and input handling
- shell_process.c     : Shell process management
- text_buffer.c       : Terminal output buffer
- tab_manager.c       : Tab management
- external_commands.c : Command execution and built-ins
- myterm.h            : Header file with declarations
- Makefile            : Build configuration

6. Notes
--------
- Ensure that .myterm_history file exists in your home directory
- All temporary files (.temp.PID.txt) are automatically deleted after termination
- For Unicode input, make sure locale is set (use export LANG=en_US.UTF-8)
- MyTerm requires X11 server to be running
- Use Ctrl+C to interrupt long-running commands
- Use Ctrl+Z to background commands
- History is automatically saved and persists across sessions

7. Troubleshooting
------------------
If compilation fails:
- Ensure X11 development libraries are installed
- Check that gcc compiler is available
- Verify Makefile syntax

If MyTerm doesn't start:
- Ensure X11 server is running
- Check DISPLAY environment variable
- Verify X11 permissions

If commands don't work:
- Check that external commands exist in PATH
- Verify file permissions for input/output redirection
- Ensure proper syntax for pipes and redirection

8. Cleaning Up
--------------
To remove object files and temporary builds:
```
make clean
```

To remove history file:
```
rm -f .myterm_history
```

9. Performance Tips
-------------------
- MyTerm is optimized for smooth operation
- Large history files may slow down startup
- Use Ctrl+C to stop unresponsive commands
- Background long-running processes with Ctrl+Z

10. Security Considerations
---------------------------
- MyTerm runs commands with user privileges
- Be cautious with input redirection from untrusted files
- Temporary files are automatically cleaned up
- History file may contain sensitive information
