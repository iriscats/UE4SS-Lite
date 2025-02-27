#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/UClass.hpp>
#include <UnrealCustom/CustomProperty.hpp>

namespace RC::Unreal
{
    CustomProperty::CustomProperty(int32_t offset_internal, int32_t element_size)
    {
        GetOffset_Internal() = offset_internal;
        GetElementSize() = element_size;
    }

    auto CustomProperty::construct(int32_t offset_internal, UClass* belongs_to_class, UClass* inner_class, int32_t element_size) -> std::unique_ptr<CustomProperty>
    {
        std::unique_ptr<CustomProperty> custom_property = std::make_unique<CustomProperty>(offset_internal, element_size);

        if (Version::IsAtLeast(4, 25))
        {
            custom_property->SetClass(inner_class);
            custom_property->SetOwnerVariant(belongs_to_class);
        }
        else
        {
            std::bit_cast<UObject*>(custom_property.get())->GetClassPrivate() = inner_class;
            std::bit_cast<UObject*>(custom_property.get())->GetOuterPrivate() = belongs_to_class;
        }

        return custom_property;
    }

    CustomArrayProperty::CustomArrayProperty(int32_t offset_internal, int32_t element_size) : CustomProperty(offset_internal, element_size)
    {
    }

    auto CustomArrayProperty::construct(int32_t offset_internal, FProperty* array_inner, int32_t element_size) -> std::unique_ptr<CustomProperty>
    {
        std::unique_ptr<CustomArrayProperty> custom_array_property = std::make_unique<CustomArrayProperty>(offset_internal, element_size);
        std::bit_cast<FArrayProperty*>(custom_array_property.get())->GetInner() = array_inner;

        return custom_array_property;
    }

    auto CustomArrayProperty::construct(int32_t offset_internal, UClass* belongs_to_class, UClass* inner_class, FProperty* array_inner, int32_t element_size)
            -> std::unique_ptr<CustomProperty>
    {
        // Create dummy property to act as the 'Inner' member variable for the ArrayProperty
        // Then we set the inner type for the array property as the 'FFieldClass' of this dummy property
        CustomProperty* array_inner_property = new CustomProperty{0x0, element_size};
        array_inner_property->SetClass(array_inner);

        std::unique_ptr<CustomProperty> custom_array_property = CustomProperty::construct(offset_internal, belongs_to_class, inner_class, element_size);
        std::bit_cast<FArrayProperty*>(custom_array_property.get())->GetInner() = array_inner_property;

        return custom_array_property;
    }

    CustomStructProperty::CustomStructProperty(int32_t offset_internal, int32_t element_size) : CustomProperty(offset_internal, element_size)
    {
    }

    auto CustomStructProperty::construct(int32_t offset_internal, UScriptStruct* script_struct, int32_t element_size) -> std::unique_ptr<CustomProperty>
    {
        std::unique_ptr<CustomStructProperty> custom_struct_property = std::make_unique<CustomStructProperty>(offset_internal, element_size);
        std::bit_cast<FStructProperty*>(custom_struct_property.get())->GetStruct() = script_struct;

        return custom_struct_property;
    }
} // namespace RC::Unreal