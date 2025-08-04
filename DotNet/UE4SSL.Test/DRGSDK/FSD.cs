using UE4SSL.Framework;
using UE4SSL.Test.DRGSDK;

public struct UGameFunctionLibrary
{
}

public class GameFunctionLibrary(IntPtr pointer) : ObjectBase<UGameFunctionLibrary>(pointer)
{
    public static GameFunctionLibrary? GetInstance()
    {
        var ptr = Find("/Script/FSD.Default__GameFunctionLibrary");
        return ptr is null ? null : new GameFunctionLibrary(ptr.Pointer);
    }

    public FSDGameMode GetFSDGameMode(ObjectReference worldContextObject)
    {
        Span<(string name, object value)> @params =
        [
            ("WorldContextObject", worldContextObject)
        ];
        return new FSDGameMode(ProcessEvent<IntPtr>(GetFunction("GetFSDGameMode")!, @params));
    }

    public FSDGameState GetFSDGameState(ObjectReference worldContextObject)
    {
        Span<(string name, object value)> @params =
        [
            ("WorldContextObject", worldContextObject)
        ];
        return new FSDGameState(ProcessEvent<IntPtr>(GetFunction("GetFSDGameState")!, @params));
    }
}


public class FSDGameState(IntPtr pointer) : ObjectBase<UGameFunctionLibrary>(pointer)
{
    public void PostGameMessage(string msg)
    {
        Span<(string name, object value)> @params =
        [
            ("Msg", FString.makeString(msg))
        ];
        ProcessEvent(GetFunction("PostGameMessage")!, @params);
    }
}


public class FSDGameMode(IntPtr pointer) : ObjectBase<UGameFunctionLibrary>(pointer)
{
    public DifficultyManager GetDifficultyManager()
    {
        Span<(string name, object value)> @params =[];
        return new DifficultyManager(ProcessEvent<IntPtr>(GetFunction("GetDifficultyManager")!, @params));
    }
}


public class DifficultyManager(IntPtr pointer) : ObjectBase<UGameFunctionLibrary>(pointer)
{
    
    public DifficultySetting GetCurrentDifficulty()
    {
        Span<(string name, object value)> @params = [];
        return new DifficultySetting(ProcessEvent<IntPtr>(GetFunction("GetCurrentDifficulty")!, @params));
    }
    
}

public class DifficultySetting(IntPtr pointer) : ObjectBase<UGameFunctionLibrary>(pointer)
{
    public float[] EnemyDamageResistance
    {
        get
        {
            UnArray array = new UnArray();
            GetArray("EnemyDamageResistance", ref array);
            return array.DataToArray<float>();
        }
        set
        {
            UnArray array = new UnArray();
            array.ArrayToData(value);
            SetArray("EnemyDamageResistance", array);            
        }
    }
}

