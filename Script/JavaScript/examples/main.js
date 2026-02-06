/**
 * Example JavaScript script for UE4SS JSScript Mod
 * 
 * Place this file in: Mods/<YourModName>/js/main.js
 * 
 * Available Global Functions:
 * - print(...args) - Print to UE4SS console
 * - FindFirstOf(className) - Find first object of class (returns null if not found)
 * - FindAllOf(className) - Find all objects of class (returns empty array if none)
 * - StaticFindObject(path) - Find object by full path
 * - RegisterHook(functionPath, preCallback, postCallback) - Register a UFunction hook by path
 * - HookUFunction(ufunctionObj, preCallback, postCallback) - Register a UFunction hook by object
 * - UnregisterHook(preId, postId) - Unregister a previously registered hook
 * - NotifyOnNewObject(className, callback) - Get notified when object is created
 * - RegisterKeyBind(key, callback, modifiers) - Register a keyboard shortcut
 * - CallFunction(object, functionName, ...args) - Call a UFunction on an object
 * - setTimeout(callback, delayMs) - Execute callback after delay
 * - setInterval(callback, intervalMs) - Execute callback repeatedly
 * - clearTimeout(id) - Cancel a setTimeout
 * - clearInterval(id) - Cancel a setInterval
 * 
 * Available Objects:
 * - UE4SS.version - UE4SS JS version string
 * 
 * Hook Callback Parameters:
 * - preCallback(thisObject, paramsArray, returnValuePtr) - Called before function execution
 * - postCallback(thisObject, paramsArray, returnValuePtr) - Called after function execution
 * 
 * Key Names for RegisterKeyBind:
 * - Letters: "A" - "Z"
 * - Numbers: "0" - "9"
 * - Function keys: "F1" - "F12"
 * - Special: "SPACE", "ENTER", "ESC", "TAB", "BACKSPACE", "DELETE", "INSERT"
 * - Navigation: "HOME", "END", "PAGEUP", "PAGEDOWN", "UP", "DOWN", "LEFT", "RIGHT"
 * - Numpad: "NUM0" - "NUM9"
 */

// Simple print test
print("Hello from JavaScript!");
print("UE4SS JS Version:", UE4SS.version);

// ============================================
// UFunction Hook Example (similar to C# Hooking.HookUFunction)
// ============================================

// Example 1: Hook by function path using RegisterHook
// This is equivalent to C#:
//   var func = ObjectReference.Find("/Game/SomeBlueprint.SomeBlueprint_C:SomeFunction");
//   Hooking.HookUFunction(func, PreCallback, PostCallback);

/*
// Pre-hook callback - called before the function executes
function onPreHook(thisObject, params, retValue) {
    print("Pre-hook called!");
    print("  This object:", thisObject);
    // params is an array of raw pointers to function parameters
    // retValue is a raw pointer to the return value location
}

// Post-hook callback - called after the function executes
function onPostHook(thisObject, params, retValue) {
    print("Post-hook called!");
}

// Register hook by function path
// Replace with actual UFunction path from your game
var hookIds = RegisterHook(
    "/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnInitCave",
    onPreHook,    // Pre-hook callback (or null to skip)
    onPostHook    // Post-hook callback (or null to skip)
);

if (hookIds) {
    print("Hook registered! Pre ID:", hookIds[0], "Post ID:", hookIds[1]);
}
*/

// Example 2: Hook using StaticFindObject + HookUFunction
// This is closer to the C# pattern:
//   var func = ObjectReference.Find(path);
//   Hooking.HookUFunction(func, preCallback, postCallback);

/*
setTimeout(function() {
    var funcPath = "/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnExecuteScript";
    var ufunc = StaticFindObject(funcPath);
    
    if (ufunc) {
        print("Found UFunction:", funcPath);
        
        var hookIds = HookUFunction(ufunc, 
            function(thisObj, params, ret) {
                print("OnExecuteScript Pre-hook fired!");
            },
            function(thisObj, params, ret) {
                print("OnExecuteScript Post-hook fired!");
            }
        );
        
        print("HookUFunction registered! IDs:", hookIds[0], hookIds[1]);
    } else {
        print("UFunction not found:", funcPath);
    }
}, 5000);
*/

// ============================================
// Example 3: Loop until function is available (like C# Update pattern)
// ============================================

// Track if we've already hooked
var isHooked = false;
var checkCount = 0;

// Pre-hook callback
function onPreExecuteScript(thisObj, params, ret) {
    print("OnExecuteScript Pre-hook fired!");
    // params[0] is now automatically converted to JS string if it's FString
    if (params && params.length > 0) {
        print("  Script parameter:", params[0]);
    }
}

// Post-hook callback  
function onPostExecuteScript(thisObj, params, ret) {
    print("OnExecuteScript Post-hook fired!");
    if (params && params.length > 0) {
        print("  Script parameter:", params[0]);
    }
}

// Hook registration function - tries to hook if function exists
function tryHook() {
    if (isHooked) {
        return true;  // Already hooked
    }

    checkCount = checkCount + 1;
    
    // Try to find the UFunction
    var funcPath = "/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnExecuteScript";
    
    try {
        var ufunc = StaticFindObject(funcPath);
        
        if (!ufunc) {
            // Function not found yet, will retry
            if (checkCount % 10 === 0) {
                print("Still waiting for UFunction... (check #" + checkCount + ")");
            }
            return false;
        }

        // Function found, register the hook
        print("Found UFunction:", funcPath);
        
        var hookIds = HookUFunction(ufunc, onPreExecuteScript, onPostExecuteScript);
        
        if (hookIds && (hookIds[0] !== 0 || hookIds[1] !== 0)) {
            print("Hook registered! Pre ID:", hookIds[0], ", Post ID:", hookIds[1]);
            isHooked = true;
            return true;
        }
    } catch (e) {
        print("Error in tryHook:", e);
    }
    
    return false;
}

// Start polling loop - check every 1000ms until hook is successful
var hookCheckInterval = setInterval(function() {
    try {
        if (tryHook()) {
            // Hook successful, stop the polling loop
            clearInterval(hookCheckInterval);
            print("Hook setup complete, stopped polling.");
        }
    } catch (e) {
        print("Error in interval:", e);
    }
}, 1000);

print("Started hook polling loop...");

// ============================================
// Example 4: 蓝图消息传送（服务端 -> 所有客户端）
// ============================================
//
// 如果 OnExecuteScript 在蓝图里设置了 Replicates: Multicast + Reliable，
// CallFunction 会检测到 FUNC_Net 标志，自动将 ProcessEvent 排队到游戏线程执行。
// 在游戏线程上 ProcessEvent 会走 UE4 的 RPC 路径，触发网络复制。

// Register F5 key to call OnExecuteScript (auto-detected as Net function -> game thread RPC)
RegisterKeyBind("F5", function() {
    print("F5 pressed! Calling OnExecuteScript (will be queued to game thread for RPC)...");
    
    try {
        var spawnMods = FindFirstOf("MI_SpawnMods_C");
        
        if (spawnMods) {
            print("Found MI_SpawnMods_C:", spawnMods.GetFullName());
            // CallFunction automatically detects FUNC_Net and queues to game thread
            var result = CallFunction(spawnMods, "OnExecuteScript", "Hello from JavaScript!");
            print("CallFunction result (queued):", result);
        } else {
            print("MI_SpawnMods_C not found.");
        }
    } catch (e) {
        print("Error in F5 handler:", e);
    }
});

// Register Ctrl+F6 to do something
RegisterKeyBind("F6", function() {
    print("Ctrl+F6 pressed! Searching for GameEngine...");
    var engine = FindFirstOf("GameEngine");
    if (engine) {
        print("Found:", engine.GetFullName());
    }
}, { ctrl: true });

// Register Ctrl+Shift+R to reload (example)
RegisterKeyBind("R", function() {
    print("Ctrl+Shift+R pressed!");
}, { ctrl: true, shift: true });

print("Key bindings registered: F5, Ctrl+F6, Ctrl+Shift+R");