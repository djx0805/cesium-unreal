// Copyright 2020-2024 CesiumGS, Inc. and Contributors

#include "CesiumMetadataValue.h"
#include "CesiumPropertyArray.h"
#include "UnrealMetadataConversions.h"
#include <CesiumGltf/MetadataConversions.h>
#include <CesiumGltf/PropertyTypeTraits.h>

FCesiumMetadataValue::FCesiumMetadataValue(FCesiumMetadataValue&& rhs) =
    default;

FCesiumMetadataValue&
FCesiumMetadataValue::operator=(FCesiumMetadataValue&& rhs) = default;

FCesiumMetadataValue::FCesiumMetadataValue(const FCesiumMetadataValue& rhs)
    : _value(),
      _valueType(rhs._valueType),
      _storage(rhs._storage),
      _pEnumDefinition(rhs._pEnumDefinition) {
  swl::visit(
      [this](const auto& value) {
        if constexpr (CesiumGltf::IsMetadataArray<decltype(value)>::value) {
          if (!this->_storage.empty()) {
            this->_value = decltype(value)(this->_storage);
          } else {
            this->_value = value;
          }
        } else {
          this->_value = value;
        }
      },
      rhs._value);
}

FCesiumMetadataValue&
FCesiumMetadataValue::operator=(const FCesiumMetadataValue& rhs) {
  *this = FCesiumMetadataValue(rhs);
  return *this;
}

ECesiumMetadataBlueprintType
UCesiumMetadataValueBlueprintLibrary::GetBlueprintType(
    UPARAM(ref) const FCesiumMetadataValue& Value) {
  return CesiumMetadataValueTypeToBlueprintType(Value._valueType);
}

ECesiumMetadataBlueprintType
UCesiumMetadataValueBlueprintLibrary::GetArrayElementBlueprintType(
    UPARAM(ref) const FCesiumMetadataValue& Value) {
  if (!Value._valueType.bIsArray) {
    return ECesiumMetadataBlueprintType::None;
  }

  FCesiumMetadataValueType types(Value._valueType);
  types.bIsArray = false;

  return CesiumMetadataValueTypeToBlueprintType(types);
}

FCesiumMetadataValueType UCesiumMetadataValueBlueprintLibrary::GetValueType(
    UPARAM(ref) const FCesiumMetadataValue& Value) {
  return Value._valueType;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

ECesiumMetadataTrueType_DEPRECATED
UCesiumMetadataValueBlueprintLibrary::GetTrueType(
    UPARAM(ref) const FCesiumMetadataValue& Value) {
  return CesiumMetadataValueTypeToTrueType(Value._valueType);
}

ECesiumMetadataTrueType_DEPRECATED
UCesiumMetadataValueBlueprintLibrary::GetTrueComponentType(
    UPARAM(ref) const FCesiumMetadataValue& Value) {
  FCesiumMetadataValueType type = Value._valueType;
  type.bIsArray = false;
  return CesiumMetadataValueTypeToTrueType(type);
}

bool UCesiumMetadataValueBlueprintLibrary::GetBoolean(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    bool DefaultValue) {
  return swl::visit(
      [DefaultValue](auto value) -> bool {
        return CesiumGltf::MetadataConversions<bool, decltype(value)>::convert(
                   value)
            .value_or(DefaultValue);
      },
      Value._value);
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

uint8 UCesiumMetadataValueBlueprintLibrary::GetByte(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    uint8 DefaultValue) {
  return swl::visit(
      [DefaultValue](auto value) -> uint8 {
        return CesiumGltf::MetadataConversions<uint8, decltype(value)>::convert(
                   value)
            .value_or(DefaultValue);
      },
      Value._value);
}

int32 UCesiumMetadataValueBlueprintLibrary::GetInteger(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    int32 DefaultValue) {
  return swl::visit(
      [DefaultValue](auto value) {
        return CesiumGltf::MetadataConversions<int32, decltype(value)>::convert(
                   value)
            .value_or(DefaultValue);
      },
      Value._value);
}

int64 UCesiumMetadataValueBlueprintLibrary::GetInteger64(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    int64 DefaultValue) {
  return swl::visit(
      [DefaultValue](auto value) -> int64 {
        return CesiumGltf::MetadataConversions<int64_t, decltype(value)>::
            convert(value)
                .value_or(DefaultValue);
      },
      Value._value);
}

float UCesiumMetadataValueBlueprintLibrary::GetFloat(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    float DefaultValue) {
  return swl::visit(
      [DefaultValue](auto value) -> float {
        return CesiumGltf::MetadataConversions<float, decltype(value)>::convert(
                   value)
            .value_or(DefaultValue);
      },
      Value._value);
}

double UCesiumMetadataValueBlueprintLibrary::GetFloat64(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    double DefaultValue) {
  return swl::visit(
      [DefaultValue](auto value) -> double {
        return CesiumGltf::MetadataConversions<double, decltype(value)>::
            convert(value)
                .value_or(DefaultValue);
      },
      Value._value);
}

FIntPoint UCesiumMetadataValueBlueprintLibrary::GetIntPoint(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FIntPoint& DefaultValue) {
  return swl::visit(
      [&DefaultValue](auto value) -> FIntPoint {
        if constexpr (CesiumGltf::IsMetadataString<decltype(value)>::value) {
          return UnrealMetadataConversions::toIntPoint(value, DefaultValue);
        } else {
          auto maybeVec2 = CesiumGltf::
              MetadataConversions<glm::ivec2, decltype(value)>::convert(value);
          return maybeVec2 ? UnrealMetadataConversions::toIntPoint(*maybeVec2)
                           : DefaultValue;
        }
      },
      Value._value);
}

FVector2D UCesiumMetadataValueBlueprintLibrary::GetVector2D(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FVector2D& DefaultValue) {
  return swl::visit(
      [&DefaultValue](auto value) -> FVector2D {
        if constexpr (CesiumGltf::IsMetadataString<decltype(value)>::value) {
          return UnrealMetadataConversions::toVector2D(value, DefaultValue);
        } else {
          auto maybeVec2 = CesiumGltf::
              MetadataConversions<glm::dvec2, decltype(value)>::convert(value);
          return maybeVec2 ? UnrealMetadataConversions::toVector2D(*maybeVec2)
                           : DefaultValue;
        }
      },
      Value._value);
}

FIntVector UCesiumMetadataValueBlueprintLibrary::GetIntVector(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FIntVector& DefaultValue) {
  return swl::visit(
      [&DefaultValue](auto value) -> FIntVector {
        if constexpr (CesiumGltf::IsMetadataString<decltype(value)>::value) {
          return UnrealMetadataConversions::toIntVector(value, DefaultValue);
        } else {
          auto maybeVec3 = CesiumGltf::
              MetadataConversions<glm::ivec3, decltype(value)>::convert(value);
          return maybeVec3 ? UnrealMetadataConversions::toIntVector(*maybeVec3)
                           : DefaultValue;
        }
      },
      Value._value);
}

FVector3f UCesiumMetadataValueBlueprintLibrary::GetVector3f(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FVector3f& DefaultValue) {
  return swl::visit(
      [&DefaultValue](auto value) -> FVector3f {
        if constexpr (CesiumGltf::IsMetadataString<decltype(value)>::value) {
          return UnrealMetadataConversions::toVector3f(value, DefaultValue);
        } else {
          auto maybeVec3 = CesiumGltf::
              MetadataConversions<glm::vec3, decltype(value)>::convert(value);
          return maybeVec3 ? UnrealMetadataConversions::toVector3f(*maybeVec3)
                           : DefaultValue;
        }
      },
      Value._value);
}

FVector UCesiumMetadataValueBlueprintLibrary::GetVector(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FVector& DefaultValue) {
  return swl::visit(
      [&DefaultValue](auto value) -> FVector {
        if constexpr (CesiumGltf::IsMetadataString<decltype(value)>::value) {
          return UnrealMetadataConversions::toVector(value, DefaultValue);
        } else {
          auto maybeVec3 = CesiumGltf::
              MetadataConversions<glm::dvec3, decltype(value)>::convert(value);
          return maybeVec3 ? UnrealMetadataConversions::toVector(*maybeVec3)
                           : DefaultValue;
        }
      },
      Value._value);
}

FVector4 UCesiumMetadataValueBlueprintLibrary::GetVector4(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FVector4& DefaultValue) {
  return swl::visit(
      [&DefaultValue](auto value) -> FVector4 {
        if constexpr (CesiumGltf::IsMetadataString<decltype(value)>::value) {
          return UnrealMetadataConversions::toVector4(value, DefaultValue);
        } else {
          auto maybeVec4 = CesiumGltf::
              MetadataConversions<glm::dvec4, decltype(value)>::convert(value);
          return maybeVec4 ? UnrealMetadataConversions::toVector4(*maybeVec4)
                           : DefaultValue;
        }
      },
      Value._value);
}

FMatrix UCesiumMetadataValueBlueprintLibrary::GetMatrix(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FMatrix& DefaultValue) {
  auto maybeMat4 = swl::visit(
      [&DefaultValue](auto value) -> std::optional<glm::dmat4> {
        return CesiumGltf::MetadataConversions<glm::dmat4, decltype(value)>::
            convert(value);
      },
      Value._value);

  return maybeMat4 ? UnrealMetadataConversions::toMatrix(*maybeMat4)
                   : DefaultValue;
}

FString UCesiumMetadataValueBlueprintLibrary::GetString(
    UPARAM(ref) const FCesiumMetadataValue& Value,
    const FString& DefaultValue) {
  return swl::visit(
      [&DefaultValue, &Value](auto value) -> FString {
        using ValueType = decltype(value);
        if constexpr (
            CesiumGltf::IsMetadataVecN<ValueType>::value ||
            CesiumGltf::IsMetadataMatN<ValueType>::value ||
            CesiumGltf::IsMetadataString<ValueType>::value) {
          return UnrealMetadataConversions::toString(value);
        } else {
          if constexpr (CesiumGltf::IsMetadataInteger<ValueType>::value) {
            if (Value._pEnumDefinition.IsValid()) {
              TOptional<FString> MaybeName =
                  Value._pEnumDefinition->GetName(value);
              if (MaybeName.IsSet()) {
                return MaybeName.GetValue();
              } else {
                return DefaultValue;
              }
            }
          }

          auto maybeString = CesiumGltf::
              MetadataConversions<std::string, decltype(value)>::convert(value);

          return maybeString ? UnrealMetadataConversions::toString(*maybeString)
                             : DefaultValue;
        }
      },
      Value._value);
}

FCesiumPropertyArray UCesiumMetadataValueBlueprintLibrary::GetArray(
    UPARAM(ref) const FCesiumMetadataValue& Value) {
  return swl::visit(
      [&EnumDefinition =
           Value._pEnumDefinition](auto value) -> FCesiumPropertyArray {
        if constexpr (CesiumGltf::IsMetadataArray<decltype(value)>::value) {
          return FCesiumPropertyArray(value, EnumDefinition);
        }
        return FCesiumPropertyArray();
      },
      Value._value);
}

bool UCesiumMetadataValueBlueprintLibrary::IsEmpty(
    UPARAM(ref) const FCesiumMetadataValue& Value) {
  return swl::holds_alternative<swl::monostate>(Value._value);
}

TMap<FString, FString> UCesiumMetadataValueBlueprintLibrary::GetValuesAsStrings(
    const TMap<FString, FCesiumMetadataValue>& Values) {
  TMap<FString, FString> strings;
  for (auto valuesIt : Values) {
    strings.Add(
        valuesIt.Key,
        UCesiumMetadataValueBlueprintLibrary::GetString(
            valuesIt.Value,
            FString()));
  }

  return strings;
}

uint64 CesiumMetadataValueAccess::GetUnsignedInteger64(
    const FCesiumMetadataValue& Value,
    uint64 DefaultValue) {
  return swl::visit(
      [DefaultValue](auto value) -> uint64 {
        return CesiumGltf::MetadataConversions<uint64_t, decltype(value)>::
            convert(value)
                .value_or(DefaultValue);
      },
      Value._value);
}
