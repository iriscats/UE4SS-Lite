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
 * - RegisterHook(functionName, callback) - Register a UFunction hook
 * - NotifyOnNewObject(className, callback) - Get notified when object is created
 * - setTimeout(callback, delayMs) - Execute callback after delay
 * - setInterval(callback, intervalMs) - Execute callback repeatedly
 * - clearTimeout(id) - Cancel a setTimeout
 * - clearInterval(id) - Cancel a setInterval
 * 
 * Available Objects:
 * - UE4SS.version - UE4SS JS version string
 */

// Simple print test
print("Hello from JavaScript!");
print("UE4SS JS Version:", UE4SS.version);

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
