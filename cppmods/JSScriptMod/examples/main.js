/**
 * Example JavaScript script for UE4SS JSScript Mod
 * 
 * Place this file in: Mods/JSScripts/js/main.js
 * 
 * Available Global Functions:
 * - print(...args) - Print to UE4SS console
 * - FindFirstOf(className) - Find first object of class
 * - FindAllOf(className) - Find all objects of class
 * - StaticFindObject(path) - Find object by full path
 * - RegisterHook(functionName, callback) - Register a UFunction hook
 * - NotifyOnNewObject(className, callback) - Get notified when object is created
 * 
 * Available Objects:
 * - UE4SS.version - UE4SS JS version string
 */

// Simple print test
print("Hello from JavaScript!");
print("UE4SS JS Version:", UE4SS.version);

// Find objects example
const gameEngine = FindFirstOf("GameEngine");
if (gameEngine) {
    print("Found GameEngine:", gameEngine.GetFullName());
    print("GameEngine Address:", gameEngine.GetAddress());
    print("GameEngine IsValid:", gameEngine.IsValid());
} else {
    print("GameEngine not found (this is normal during early loading)");
}

// Find all players example
const players = FindAllOf("PlayerController");
if (players && players.length > 0) {
    print("Found", players.length, "PlayerController(s)");
    for (let i = 0; i < players.length; i++) {
        print("  Player", i + 1, ":", players[i].GetName());
    }
} else {
    print("No PlayerControllers found yet");
}

// Example of using StaticFindObject
// const specificObject = StaticFindObject("/Game/Path/To/Object.Object");

// Hook registration example (simplified - full implementation requires more work)
// RegisterHook("/Script/Engine.PlayerController:ClientRestart", (context) => {
//     print("PlayerController ClientRestart called!");
// });

print("JavaScript script loaded successfully!");
