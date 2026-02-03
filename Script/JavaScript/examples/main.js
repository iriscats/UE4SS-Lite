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
// Simple delayed execution test
// ============================================

// Wait 3 seconds, then try to find GameEngine
setTimeout(function() {
    print("=== 3 seconds passed ===");
    
    var gameEngine = FindFirstOf("GameEngine");
    if (gameEngine) {
        print("Found GameEngine!");
    } else {
        print("GameEngine not found");
    }
}, 3000);

// Another test at 5 seconds
setTimeout(function() {
    print("=== 5 seconds passed ===");
    
    var players = FindAllOf("PlayerController");
    if (players && players.length > 0) {
        print("Found " + players.length + " PlayerController(s)");
    } else {
        print("No PlayerControllers found");
    }
}, 5000);

print("JavaScript script loaded successfully!");
print("Waiting for delayed checks (3s and 5s)...");
