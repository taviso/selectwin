# selectwin

A minimal commandline utility for interactively selecting a window.

If you want to prompt the user to select a window and get a HWND to pass to
another utility, you can do this:

```
selectwin.exe otherutil.exe /handle @@
```

The `otherutil.exe` utility will be executed, and the `@@` will be replaced
with the handle number.

A real example I use:

```
selectwin nircmd win settopmost handle @@ 1
```
