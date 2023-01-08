# Debugging Notes for WinPX

## Testing the proxy installation step-by-step

To test the proxy installation, you can use the `curl` command-line tool, which is included with **Windows 10** and later versions.

Open a **Windows Command Prompt** window and try executing the following commands.

First, let's check if you have a direct connection to the Internet:

```bat
curl --noproxy * --head http://example.com
```

The `--noproxy *` option tells `curl` to bypass any configured proxy.
If the first line of the response starts with `HTTP/1.1 200 OK`, then you have a direct connection to `example.com`,
and you may not need an intermediate proxy at all.

In a corporate network environment employing an enterprise proxy server, you are likely to receive an error message, such as:

- `curl: (6) Could not resolve host: example.com`
- `curl: (7) Failed to connect to example.com:80 after 2047 ms: Could not connect to server`

You can try to use the `curl` command with the `--proxy` option to specify the proxy server.
For instance, if your enterprise proxy server is `http://proxy.contoso.com:8080`, you can try the following command:

```bat
curl --proxy http://proxy.contoso.com:8080 --head http://example.com
```

A typical response from the proxy server is `HTTP/1.1 407 Proxy Authentication Required`,
which indicates that the proxy server requires authentication before allowing access to the requested resource.

To authenticate with the proxy server, the most secure option is **Negotiate** authentication (SPNEGO) using Kerberos.
You can tell `curl` to use **Negotiate** authentication with the `--proxy-negotiate` option:

```bat
curl --proxy http://proxy.contoso.com:8080 --proxy-negotiate --head http://example.com
```

Running the above command, you should get the expected response `HTTP/1.1 200 OK`.

If some of your applications do not support Negotiate authentication or any of the proxy discovery protocols your company uses,
you can use **WinPX** as an intermediate proxy server to forward the requests to the enterprise proxy server.

Start the **WinPX** proxy server by running the `winpx.exe` executable.

By default, **WinPX** will automatically configure itself based on the Windows proxy configuration.
It will then listen for HTTP requests on the address `http://127.0.0.1:3128`.

Now, you can try the `curl` command again, but this time using the **WinPX** proxy server:

```bat
curl --proxy http://127.0.0.1:3128 --head http://example.com
```

Running the above command, you should again get the response `HTTP/1.1 200 OK`.

Compared to using the enterprise proxy server directly, **WinPX** automatically
detects the appropriate proxy settings and handles the proxy authentication for you.

Note that the address that **WinPX** listens on may differ from this example. To find out the actual proxy server address,
you can open the **WinPX Options** dialog by double-clicking the **WinPX** icon in the Windows system tray.
Then click the **Copy proxy URL to clipboard** button to copy the proxy URL to the clipboard.

## Viewing the Windows System Proxy Configuration

To view the proxy settings that are configured for your user account, run the following command in a Windows terminal:

```bat
netsh winhttp show advproxy
```
