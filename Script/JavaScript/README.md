# JSScriptMod - JavaScript Scripting for UE4SS

JSScriptMod provides JavaScript scripting support for UE4SS using the QuickJS engine, similar to the built-in Lua scripting support.

## Features

- ES2023 JavaScript support via QuickJS engine
- UE4 object access through familiar API
- Module system with ES6 `import`/`export`
- Similar API to Lua scripting (FindFirstOf, FindAllOf, etc.)

## Installation

1. Build the JSScriptMod project as part of UE4SS
2. The `JSScriptMod.dll` will be created in the output directory
3. Create your JavaScript scripts in `Mods/JSScripts/js/`

## Directory Structure

```
Mods/
└── JSScripts/
    └── js/
        ├── main.js          # Entry point script (required)
        └── modules/         # Optional module directory
            └── utils.js
```

## API Reference

### Global Functions

#### `print(...args)`
Print values to the UE4SS console.

```javascript
print("Hello", "World", 123);
```

#### `FindFirstOf(className)`
Find the first object of a given class.

```javascript
const engine = FindFirstOf("GameEngine");
if (engine) {
    print(engine.GetFullName());
}
```

#### `FindAllOf(className)`
Find all objects of a given class.

```javascript
const players = FindAllOf("PlayerController");
for (const player of players) {
    print(player.GetName());
}
```

#### `StaticFindObject(path)`
Find an object by its full path.

```javascript
const obj = StaticFindObject("/Script/Engine.Default__GameEngine");
```

#### `RegisterHook(functionName, callback)`
Register a hook for a UFunction.

```javascript
RegisterHook("/Script/Engine.PlayerController:ClientRestart", (context) => {
    print("ClientRestart called!");
});
```

#### `NotifyOnNewObject(className, callback)`
Get notified when a new object of the specified class is created.

```javascript
NotifyOnNewObject("PlayerController", (newObject) => {
    print("New PlayerController created:", newObject.GetName());
});
```

### UObject Methods

When you get a UObject from `FindFirstOf`, `FindAllOf`, etc., you can use these methods:

- `GetFullName()` - Get the full name of the object
- `GetName()` - Get the short name of the object
- `GetClass()` - Get the UClass of the object
- `IsA(className)` - Check if object is instance of a class
- `GetAddress()` - Get the memory address (for debugging)
- `IsValid()` - Check if the object is still valid

### Global Objects

#### `UE4SS`
```javascript
print(UE4SS.version);  // "1.0.0"
```

## Module System

JSScriptMod supports ES6 modules. Create modules in your scripts directory:

**utils.js**
```javascript
export function helper() {
    return "Hello from helper";
}

export const VERSION = "1.0.0";
```

**main.js**
```javascript
import { helper, VERSION } from './utils.js';

print(helper());
print("Version:", VERSION);
```

## Building

JSScriptMod is built as part of the UE4SS cppmods. Make sure you have:

1. CMake 3.15+
2. C++20 compatible compiler

The QuickJS source is included in `deps/quickjs/`.

## Limitations

- Hook system is simplified compared to Lua (full integration WIP)
- Not all UE4 types are bound yet
- Property access on UObjects is limited

## License

QuickJS is licensed under MIT. See `deps/quickjs/LICENSE` for details.
