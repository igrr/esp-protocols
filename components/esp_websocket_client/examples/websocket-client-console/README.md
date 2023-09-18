# Console over Websockets

This example demonstrates a console running on an ESP chip, accessible over Websockets. It also serves as an example of creating a custom VFS driver and using this driver to provide `stdin`/`stdout` streams for the console.

In more detail, the example does the following:

- Connects to Wi-Fi or Ethernet, as configured in menuconfig
- Connects to the specified Websocket server (URL currently hard-coded in main.c)
- Registers a VFS driver which wraps a websocket connection.
- Creates a console task.
- Opens stdin and stdout from the websocket VFS driver.
- Starts a console read-print-eval loop.

## Using the example

1. Configure Wi-Fi credentials or Ethernet settings in menuconfig
2. Edit Websocket server URL in main.c
3. Build, flash and monitor
4. Launch a Websocket server. A simple one is provided in `console.py`:
   ```bash
   python console.py --host <IP to bind to> --port 8080
   ```

   The script will accept lines from stdin and will send them over to the connected Websocket client using binary frames. Any received binary frames from the client will be echoed to the console:

   ```
   $ python console.py --host 10.0.2.83
   This is an example of ESP-IDF console component.
   Type 'help' to get the list of commands.
   Use UP/DOWN arrows to navigate through command history.
   Press TAB when typing command name to auto-complete.
   Press Enter or Ctrl+C will terminate the console environment.
   > help
   help
   help  [<string>]
   Print the summary of all registered commands if no arguments are given,
   otherwise print summary of given command.
       <string>  Name of command

   >
   ```

   You will need to run `pip install asyncio websockets aioconsole` to use this script.

