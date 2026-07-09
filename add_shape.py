import dbus
import sys

print("Connecting to Session Bus...")
try:
    bus = dbus.SessionBus()
    obj = bus.get_object('org.kde.scribbleway', '/Overlay')
    iface = dbus.Interface(obj, 'org.kde.scribbleway.OverlayController')
    
    print("Entering Select Mode via D-Bus...")
    iface.enterSelectMode()
    
    print("Calling addShape D-Bus method...")
    iface.addShape({
        'type': 'rectangle',
        'x': 100.0,
        'y': 100.0,
        'width': 300.0,
        'height': 200.0,
        'color': '#e63946', # Red
        'strokeWidth': 8.0,
        'opacity': 1.0
    })
    print("Shape added successfully!")
except Exception as e:
    print(f"Error calling D-Bus method: {e}", file=sys.stderr)
    sys.exit(1)
