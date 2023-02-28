## New
### Lua 

* Added 'ModRef.SetSharedVariable' and 'ModRef.GetSharedVariable'
* Added UObject.HasAnyInternalFlags
* Added global table: 'EInternalObjectFlags'

* You can now set an ObjectProperty value to `nil`. Previously such an action would be ignored.

* When calling 'IsValid()' on UObjects, whether the UObject is reachable is now taken into account.

* The splitscreen mod now operates independently of the Lua state which means that hot-reloading shouldn't cause it to break

* Added shared module "UEHelpers" to provide shortcut functions to the Lua module for commonly used UE functions or classes

### Live View GUI

* Default renderer of the GUI has been changed to OpenGL for compatibility reasons.  This can be changed back to dx11

## Changes

### UHT Generation

* Buckminsterfullerene - Includes and forward declarations are now ordered to allow for easier diffing.
* Buckminsterfullerene - Added setting to force "Config = Engine" on UCLASS specifiers for classes with "DefaultConfig", "GlobalUserConfig" or "ProjectUserConfig"
* Praydog - Fixes to build.cs generation
* Buckminsterfullerene - Tick functions now include the required template
* Add missed commit related to UHT gen ordering  
  
### CXX Dump

* Buckminsterfullerene - Structs and classes are now ordered to allow for easier diffing


