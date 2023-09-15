// Copyright 2020-2023 CesiumGS, Inc. and Contributors

#include "CesiumFeaturesMetadataComponent.h"
#include "Cesium3DTileset.h"
#include "CesiumEncodedFeaturesMetadata.h"
#include "CesiumEncodedMetadataConversions.h"
#include "CesiumGltfComponent.h"
#include "CesiumGltfPrimitiveComponent.h"
#include "CesiumMetadataConversions.h"
#include "CesiumModelMetadata.h"
#include "CesiumRuntime.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ComponentReregisterContext.h"
#include "Containers/Map.h"
#include "ContentBrowserModule.h"
#include "Factories/MaterialFunctionMaterialLayerFactory.h"
#include "IContentBrowserSingleton.h"
#include "IMaterialEditor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"

extern UNREALED_API class UEditorEngine* GEditor;
#endif

using namespace CesiumEncodedFeaturesMetadata;

namespace {
void AutoFillPropertyTableDescriptions(
    TArray<FCesiumPropertyTableDescription>& Descriptions,
    const FCesiumModelMetadata& ModelMetadata) {
  const TArray<FCesiumPropertyTable>& propertyTables =
      UCesiumModelMetadataBlueprintLibrary::GetPropertyTables(ModelMetadata);

  for (int32 i = 0; i < propertyTables.Num(); i++) {
    FCesiumPropertyTable propertyTable = propertyTables[i];
    FString propertyTableName = getNameForPropertyTable(propertyTable);

    FCesiumPropertyTableDescription* pDescription =
        Descriptions.FindByPredicate(
            [&name = propertyTableName](
                const FCesiumPropertyTableDescription& existingPropertyTable) {
              return existingPropertyTable.Name == name;
            });

    if (!pDescription) {
      pDescription = &Descriptions.Emplace_GetRef();
      pDescription->Name = propertyTableName;
    }

    const TMap<FString, FCesiumPropertyTableProperty>& properties =
        UCesiumPropertyTableBlueprintLibrary::GetProperties(propertyTable);
    for (const auto& propertyIt : properties) {
      auto pExistingProperty = pDescription->Properties.FindByPredicate(
          [&propertyName = propertyIt.Key](
              const FCesiumMetadataPropertyDescription& existingProperty) {
            return existingProperty.Name == propertyName;
          });

      if (pExistingProperty) {
        // We have already accounted for this property, but we may need to check
        // for its offset / scale, since they can differ from the class
        // property's definition.
        ECesiumMetadataType type = pExistingProperty->PropertyDetails.Type;
        switch (type) {
        case ECesiumMetadataType::Scalar:
        case ECesiumMetadataType::Vec2:
        case ECesiumMetadataType::Vec3:
        case ECesiumMetadataType::Vec4:
        case ECesiumMetadataType::Mat2:
        case ECesiumMetadataType::Mat3:
        case ECesiumMetadataType::Mat4:
          break;
        default:
          continue;
        }

        FCesiumMetadataValue offset =
            UCesiumPropertyTablePropertyBlueprintLibrary::GetOffset(
                propertyIt.Value);
        pExistingProperty->PropertyDetails.bHasOffset |=
            !UCesiumMetadataValueBlueprintLibrary::IsEmpty(offset);

        FCesiumMetadataValue scale =
            UCesiumPropertyTablePropertyBlueprintLibrary::GetOffset(
                propertyIt.Value);
        pExistingProperty->PropertyDetails.bHasScale |=
            !UCesiumMetadataValueBlueprintLibrary::IsEmpty(scale);

        continue;
      }

      FCesiumMetadataPropertyDescription& property =
          pDescription->Properties.Emplace_GetRef();
      property.Name = propertyIt.Key;

      const FCesiumMetadataValueType ValueType =
          UCesiumPropertyTablePropertyBlueprintLibrary::GetValueType(
              propertyIt.Value);
      property.PropertyDetails.SetValueType(ValueType);
      property.PropertyDetails.ArraySize =
          UCesiumPropertyTablePropertyBlueprintLibrary::GetArraySize(
              propertyIt.Value);
      property.PropertyDetails.bIsNormalized =
          UCesiumPropertyTablePropertyBlueprintLibrary::IsNormalized(
              propertyIt.Value);

      FCesiumMetadataValue offset =
          UCesiumPropertyTablePropertyBlueprintLibrary::GetOffset(
              propertyIt.Value);
      property.PropertyDetails.bHasOffset =
          !UCesiumMetadataValueBlueprintLibrary::IsEmpty(offset);

      FCesiumMetadataValue scale =
          UCesiumPropertyTablePropertyBlueprintLibrary::GetOffset(
              propertyIt.Value);
      property.PropertyDetails.bHasScale =
          !UCesiumMetadataValueBlueprintLibrary::IsEmpty(scale);

      FCesiumMetadataValue noData =
          UCesiumPropertyTablePropertyBlueprintLibrary::GetNoDataValue(
              propertyIt.Value);
      property.PropertyDetails.bHasNoDataValue =
          !UCesiumMetadataValueBlueprintLibrary::IsEmpty(noData);

      FCesiumMetadataValue defaultValue =
          UCesiumPropertyTablePropertyBlueprintLibrary::GetDefaultValue(
              propertyIt.Value);
      property.PropertyDetails.bHasDefaultValue =
          !UCesiumMetadataValueBlueprintLibrary::IsEmpty(defaultValue);

      property.EncodingDetails = CesiumMetadataPropertyDetailsToEncodingDetails(
          property.PropertyDetails);
    }
  }
}

void AutoFillPropertyTextureDescriptions() {
  // const TMap<FString, FCesiumFeatureTexture>& featureTextures =
  //    UCesiumMetadataModelBlueprintLibrary::GetFeatureTextures(model);

  // for (const auto& featureTextureIt : featureTextures) {
  //  FFeatureTextureDescription* pFeatureTexture =
  //      this->FeatureTextures.FindByPredicate(
  //          [&featureTextureName = featureTextureIt.Key](
  //              const FFeatureTextureDescription& existingFeatureTexture) {
  //            return existingFeatureTexture.Name == featureTextureName;
  //          });

  //  if (!pFeatureTexture) {
  //    pFeatureTexture = &this->FeatureTextures.Emplace_GetRef();
  //    pFeatureTexture->Name = featureTextureIt.Key;
  //  }

  //  const TArray<FString>& propertyNames =
  //      UCesiumFeatureTextureBlueprintLibrary::GetPropertyKeys(
  //          featureTextureIt.Value);

  //  for (const FString& propertyName : propertyNames) {
  //    if (pFeatureTexture->Properties.FindByPredicate(
  //            [&propertyName](const FFeatureTexturePropertyDescription&
  //                                existingProperty) {
  //              return propertyName == existingProperty.Name;
  //            })) {
  //      // We have already filled this property.
  //      continue;
  //    }

  //    FCesiumPropertyTextureProperty property =
  //        UCesiumPropertyTextureBlueprintLibrary::FindProperty(
  //            featureTextureIt.Value,
  //            propertyName);
  //    FFeatureTexturePropertyDescription& propertyDescription =
  //        pFeatureTexture->Properties.Emplace_GetRef();
  //    propertyDescription.Name = propertyName;
  //    propertyDescription.Normalized =
  //        UCesiumPropertyTexturePropertyBlueprintLibrary::IsNormalized(
  //            property);

  //    switch (
  //        UCesiumPropertyTexturePropertyBlueprintLibrary::GetComponentCount(
  //            property)) {
  //    case 2:
  //      propertyDescription.Type = ECesiumPropertyType::Vec2;
  //      break;
  //    case 3:
  //      propertyDescription.Type = ECesiumPropertyType::Vec3;
  //      break;
  //    case 4:
  //      propertyDescription.Type = ECesiumPropertyType::Vec4;
  //      break;
  //    // case 1:
  //    default:
  //      propertyDescription.Type = ECesiumPropertyType::Scalar;
  //    }
  //  }
  //}
}

void AutoFillFeatureIdSetDescriptions(
    TArray<FCesiumFeatureIdSetDescription>& Descriptions,
    const FCesiumPrimitiveFeatures& Features,
    const TArray<FCesiumPropertyTable>& PropertyTables) {
  const TArray<FCesiumFeatureIdSet> featureIDSets =
      UCesiumPrimitiveFeaturesBlueprintLibrary::GetFeatureIDSets(Features);
  int32 featureIDTextureCounter = 0;

  for (const FCesiumFeatureIdSet& featureIDSet : featureIDSets) {
    ECesiumFeatureIdSetType type =
        UCesiumFeatureIdSetBlueprintLibrary::GetFeatureIDSetType(featureIDSet);
    int64 count =
        UCesiumFeatureIdSetBlueprintLibrary::GetFeatureCount(featureIDSet);
    if (type == ECesiumFeatureIdSetType::None || count == 0) {
      // Empty or invalid feature ID set. Skip.
      continue;
    }

    FString featureIDSetName =
        getNameForFeatureIDSet(featureIDSet, featureIDTextureCounter);
    FCesiumFeatureIdSetDescription* pDescription = Descriptions.FindByPredicate(
        [&name = featureIDSetName](
            const FCesiumFeatureIdSetDescription& existingFeatureIDSet) {
          return existingFeatureIDSet.Name == name;
        });

    if (pDescription) {
      // We have already accounted for a feature ID set of this name; skip.
      continue;
    }

    pDescription = &Descriptions.Emplace_GetRef();
    pDescription->Name = featureIDSetName;
    pDescription->Type = type;

    const int64 propertyTableIndex =
        UCesiumFeatureIdSetBlueprintLibrary::GetPropertyTableIndex(
            featureIDSet);

    if (propertyTableIndex >= 0 && propertyTableIndex < PropertyTables.Num()) {
      const FCesiumPropertyTable& propertyTable =
          PropertyTables[propertyTableIndex];
      pDescription->PropertyTableName = getNameForPropertyTable(propertyTable);
    }

    pDescription->bHasNullFeatureId =
        UCesiumFeatureIdSetBlueprintLibrary::GetNullFeatureID(featureIDSet) >
        -1;
  }
}

} // namespace

void UCesiumFeaturesMetadataComponent::AutoFill() {
  const ACesium3DTileset* pOwner = this->GetOwner<ACesium3DTileset>();
  if (!pOwner) {
    return;
  }

  // This assumes that the property tables are the same across all models in the
  // tileset, and that they all have the same schema.
  for (const UActorComponent* pComponent : pOwner->GetComponents()) {
    const UCesiumGltfComponent* pGltf = Cast<UCesiumGltfComponent>(pComponent);
    if (!pGltf) {
      continue;
    }

    const FCesiumModelMetadata& modelMetadata = pGltf->Metadata;
    AutoFillPropertyTableDescriptions(this->PropertyTables, modelMetadata);

    TArray<USceneComponent*> childComponents;
    pGltf->GetChildrenComponents(false, childComponents);

    for (const USceneComponent* pChildComponent : childComponents) {
      const UCesiumGltfPrimitiveComponent* pGltfPrimitive =
          Cast<UCesiumGltfPrimitiveComponent>(pChildComponent);
      if (!pGltfPrimitive) {
        continue;
      }

      const FCesiumPrimitiveFeatures& primitiveFeatures =
          pGltfPrimitive->Features;
      const TArray<FCesiumPropertyTable>& propertyTables =
          UCesiumModelMetadataBlueprintLibrary::GetPropertyTables(
              modelMetadata);
      AutoFillFeatureIdSetDescriptions(
          this->FeatureIdSets,
          primitiveFeatures,
          propertyTables);
    }
  }
}

#if WITH_EDITOR
template <typename ObjClass>
static FORCEINLINE ObjClass* LoadObjFromPath(const FName& Path) {
  if (Path == NAME_None)
    return nullptr;

  return Cast<ObjClass>(
      StaticLoadObject(ObjClass::StaticClass(), nullptr, *Path.ToString()));
}

static FORCEINLINE UMaterialFunction* LoadMaterialFunction(const FName& Path) {
  if (Path == NAME_None)
    return nullptr;

  return LoadObjFromPath<UMaterialFunction>(Path);
}

namespace {
struct MaterialNodeClassification {
  TArray<UMaterialExpression*> AutoGeneratedNodes;
  TArray<UMaterialExpressionMaterialFunctionCall*> GetFeatureIdNodes;
  TArray<UMaterialExpressionCustom*> GetPropertyValueNodes;
  TArray<UMaterialExpressionIf*> IfNodes;
  TArray<UMaterialExpression*> UserAddedNodes;
};
} // namespace

// Separate nodes into auto-generated and user-added. Collect the property
// result nodes.
static void ClassifyNodes(
    UMaterialFunctionMaterialLayer* Layer,
    MaterialNodeClassification& Classification,
    UMaterialFunction* GetFeatureIdsFromAttributeFunction,
    UMaterialFunction* GetFeatureIdsFromTextureFunction) {
#if ENGINE_MINOR_VERSION == 0
  for (UMaterialExpression* Node : Layer->FunctionExpressions) {
#else
  for (const TObjectPtr<UMaterialExpression>& Node :
       Layer->GetExpressionCollection().Expressions) {
#endif
    // Check if this node is marked as autogenerated.
    if (Node->Desc.StartsWith(
            "AUTOGENERATED DO NOT EDIT",
            ESearchCase::Type::CaseSensitive)) {
      Classification.AutoGeneratedNodes.Add(Node);

      // The only auto-generated custom nodes are the property result nodes
      // (i.e., nodes named "Get Property Values From ___").
      UMaterialExpressionCustom* CustomNode =
          Cast<UMaterialExpressionCustom>(Node);
      if (CustomNode) {
        Classification.GetPropertyValueNodes.Add(CustomNode);
        continue;
      }

      // If nodes are added in when feature ID sets specify a null feature ID
      // value, when properties specify a "no data" value, and when properties
      // specify a default value.
      UMaterialExpressionIf* IfNode = Cast<UMaterialExpressionIf>(Node);
      if (IfNode) {
        Classification.IfNodes.Add(IfNode);
        continue;
      }

      UMaterialExpressionMaterialFunctionCall* FunctionCallNode =
          Cast<UMaterialExpressionMaterialFunctionCall>(Node);
      if (!FunctionCallNode)
        continue;

      const FName& name = FunctionCallNode->MaterialFunction->GetFName();
      if (name == GetFeatureIdsFromAttributeFunction->GetFName() ||
          name == GetFeatureIdsFromTextureFunction->GetFName()) {
        Classification.GetFeatureIdNodes.Add(FunctionCallNode);
      }
    } else {
      Classification.UserAddedNodes.Add(Node);
    }
  }
}

static void ClearAutoGeneratedNodes(
    UMaterialFunctionMaterialLayer* Layer,
    TMap<FString, TMap<FString, const FExpressionInput*>>& ConnectionInputRemap,
    TMap<FString, TArray<FExpressionInput*>>& ConnectionOutputRemap,
    UMaterialFunction* GetFeatureIdsFromAttributeFunction,
    UMaterialFunction* GetFeatureIdsFromTextureFunction) {

  MaterialNodeClassification Classification;
  ClassifyNodes(
      Layer,
      Classification,
      GetFeatureIdsFromAttributeFunction,
      GetFeatureIdsFromTextureFunction);

  // Determine which user-added connections to remap when regenerating the
  // feature ID retrieval nodes.
  for (const UMaterialExpressionMaterialFunctionCall* GetFeatureIdNode :
       Classification.GetFeatureIdNodes) {
    if (!GetFeatureIdNode->Outputs.Num()) {
      continue;
    }

    const auto Inputs = GetFeatureIdNode->FunctionInputs;

    if (!Inputs.Num()) {
      // Should not happen, but just in case, this node would be invalid. Break
      // any user-made connections to this node and don't attempt to remap it.
      for (UMaterialExpression* UserNode : Classification.UserAddedNodes) {
        for (FExpressionInput* Input : UserNode->GetInputs()) {
          if (Input->Expression == GetFeatureIdNode &&
              Input->OutputIndex == 0) {
            Input->Expression = nullptr;
          }
        }
      }
      continue;
    }

    // It's not as easy to distinguish the material function calls from each
    // other. Try using the name of the first valid input (the texcoord index
    // name), which should be different for each feature ID set.
    const auto Parameter =
        Cast<UMaterialExpressionParameter>(Inputs[0].Input.Expression);
    FString ParameterName;
    if (Parameter) {
      ParameterName = Parameter->ParameterName.ToString();
    }

    if (ParameterName.IsEmpty()) {
      // In case, treat the node as invalid. Break any user-made connections to
      // this node and don't attempt to remap it.
      for (UMaterialExpression* UserNode : Classification.UserAddedNodes) {
        for (FExpressionInput* Input : UserNode->GetInputs()) {
          if (Input->Expression == GetFeatureIdNode &&
              Input->OutputIndex == 0) {
            Input->Expression = nullptr;
          }
        }
      }
      continue;
    }

    FString Key = GetFeatureIdNode->GetDescription() + ParameterName;
    TArray<FExpressionInput*> Connections;
    for (UMaterialExpression* UserNode : Classification.UserAddedNodes) {
      for (FExpressionInput* Input : UserNode->GetInputs()) {
        // Look for user-made connections to this node.
        if (Input->Expression == GetFeatureIdNode && Input->OutputIndex == 0) {
          Connections.Add(Input);
          Input->Expression = nullptr;
        }
      }
    }
    ConnectionOutputRemap.Emplace(MoveTemp(Key), MoveTemp(Connections));
  }

  // Determine which user-added connections to remap when regenerating the
  // property value retrieval nodes.
  for (const UMaterialExpressionCustom* GetPropertyValueNode :
       Classification.GetPropertyValueNodes) {
    int32 OutputIndex = 0;
    for (const FExpressionOutput& PropertyOutput :
         GetPropertyValueNode->Outputs) {
      FString Key = GetPropertyValueNode->GetDescription() +
                    PropertyOutput.OutputName.ToString();

      // Look for user-made connections to this property.
      TArray<FExpressionInput*> Connections;
      for (UMaterialExpression* UserNode : Classification.UserAddedNodes) {
        for (FExpressionInput* Input : UserNode->GetInputs()) {
          if (Input->Expression == GetPropertyValueNode &&
              Input->OutputIndex == OutputIndex) {
            Connections.Add(Input);
            Input->Expression = nullptr;
          }
        }
      }

      ConnectionOutputRemap.Emplace(MoveTemp(Key), MoveTemp(Connections));
      ++OutputIndex;
    }
  }

  // Determine which user-added connections to remap when regenerating the if
  // statements for null feature IDs.
  for (const UMaterialExpressionIf* IfNode : Classification.IfNodes) {
    // Distinguish the if statements from each other using A and B. If both of
    // these nodes have been disconnected, then treat this node as invalid.
    FString IfNodeName;

    UMaterialExpressionParameter* Parameter =
        Cast<UMaterialExpressionParameter>(IfNode->A.Expression);
    if (Parameter) {
      IfNodeName += Parameter->GetParameterName().ToString();
    } else if (IfNode->A.Expression) {
      TArray<FExpressionOutput>& Outputs = IfNode->A.Expression->GetOutputs();
      if (Outputs.Num() > 0) {
        IfNodeName += Outputs[IfNode->A.OutputIndex].OutputName.ToString();
      }
    }

    Parameter = Cast<UMaterialExpressionParameter>(IfNode->B.Expression);
    if (Parameter) {
      IfNodeName += Parameter->GetParameterName().ToString();
    } else if (IfNode->B.Expression) {
      TArray<FExpressionOutput>& Outputs = IfNode->B.Expression->GetOutputs();
      if (Outputs.Num() > 0) {
        IfNodeName += Outputs[IfNode->B.OutputIndex].OutputName.ToString();
      }
    }

    if (IfNodeName.IsEmpty()) {
      // In case, treat the node as invalid. Break any user-made connections to
      // this node and don't attempt to remap it.
      for (UMaterialExpression* UserNode : Classification.UserAddedNodes) {
        for (FExpressionInput* Input : UserNode->GetInputs()) {
          if (Input->Expression == IfNode && Input->OutputIndex == 0) {
            Input->Expression = nullptr;
          }
        }
      }
      continue;
    }

    FString Key = IfNode->GetDescription() + IfNodeName;
    TArray<FExpressionInput*> Connections;
    for (UMaterialExpression* UserNode : Classification.UserAddedNodes) {
      for (FExpressionInput* Input : UserNode->GetInputs()) {
        // Look for user-made connections to this node.
        if (Input->Expression == IfNode && Input->OutputIndex == 0) {
          Connections.Add(Input);
          Input->Expression = nullptr;
        }
      }
    }
    ConnectionOutputRemap.Emplace(Key, MoveTemp(Connections));

    // Also save any user inputs to the if statement. Be sure to ignore
    // connections to autogenerated nodes.
    TMap<FString, const FExpressionInput*> InputConnections;
    if (IfNode->AGreaterThanB.Expression &&
        !IfNode->AGreaterThanB.Expression->Desc.StartsWith(
            "AUTOGENERATED DO NOT EDIT",
            ESearchCase::Type::CaseSensitive)) {
      InputConnections.Emplace(TEXT("AGreaterThanB"), &IfNode->AGreaterThanB);
    }

    if (IfNode->ALessThanB.Expression &&
        !IfNode->ALessThanB.Expression->Desc.StartsWith(
            "AUTOGENERATED DO NOT EDIT",
            ESearchCase::Type::CaseSensitive)) {
      InputConnections.Emplace(TEXT("ALessThanB"), &IfNode->ALessThanB);
    }

    if (IfNode->AEqualsB.Expression &&
        !IfNode->AEqualsB.Expression->Desc.StartsWith(
            "AUTOGENERATED DO NOT EDIT",
            ESearchCase::Type::CaseSensitive)) {
      InputConnections.Emplace(TEXT("AEqualsB"), &IfNode->AEqualsB);
    }

    ConnectionInputRemap.Emplace(MoveTemp(Key), MoveTemp(InputConnections));
  }

  // Remove auto-generated nodes.
  for (UMaterialExpression* AutoGeneratedNode :
       Classification.AutoGeneratedNodes) {
#if ENGINE_MINOR_VERSION == 0
    Layer->FunctionExpressions.Remove(AutoGeneratedNode);
#else
    Layer->GetExpressionCollection().RemoveExpression(AutoGeneratedNode);
#endif
  }
}

static void RemapUserConnections(
    UMaterialFunctionMaterialLayer* Layer,
    TMap<FString, TMap<FString, const FExpressionInput*>>& ConnectionInputRemap,
    TMap<FString, TArray<FExpressionInput*>>& ConnectionOutputRemap,
    UMaterialFunction* GetFeatureIdsFromAttributeFunction,
    UMaterialFunction* GetFeatureIdsFromTextureFunction) {

  MaterialNodeClassification Classification;
  ClassifyNodes(
      Layer,
      Classification,
      GetFeatureIdsFromAttributeFunction,
      GetFeatureIdsFromTextureFunction);

  for (UMaterialExpressionMaterialFunctionCall* GetFeatureIdNode :
       Classification.GetFeatureIdNodes) {
    const auto Inputs = GetFeatureIdNode->FunctionInputs;
    const auto Parameter =
        Cast<UMaterialExpressionParameter>(Inputs[0].Input.Expression);
    FString ParameterName = Parameter->ParameterName.ToString();

    FString Key = GetFeatureIdNode->GetDescription() + ParameterName;
    TArray<FExpressionInput*>* pConnections = ConnectionOutputRemap.Find(Key);
    if (pConnections) {
      for (FExpressionInput* pConnection : *pConnections) {
        pConnection->Expression = GetFeatureIdNode;
        pConnection->OutputIndex = 0;
      }
    }
  }

  for (UMaterialExpressionCustom* GetPropertyValueNode :
       Classification.GetPropertyValueNodes) {
    int32 OutputIndex = 0;
    for (const FExpressionOutput& PropertyOutput :
         GetPropertyValueNode->Outputs) {
      FString Key = GetPropertyValueNode->Description +
                    PropertyOutput.OutputName.ToString();

      TArray<FExpressionInput*>* pConnections = ConnectionOutputRemap.Find(Key);
      if (pConnections) {
        for (FExpressionInput* pConnection : *pConnections) {
          pConnection->Expression = GetPropertyValueNode;
          pConnection->OutputIndex = OutputIndex;
        }
      }

      ++OutputIndex;
    }
  }

  for (UMaterialExpressionIf* IfNode : Classification.IfNodes) {
    FString IfNodeName;

    FString AName;
    UMaterialExpressionParameter* Parameter =
        Cast<UMaterialExpressionParameter>(IfNode->A.Expression);
    if (Parameter) {
      AName = Parameter->GetParameterName().ToString();
    } else if (IfNode->A.Expression) {
      TArray<FExpressionOutput>& Outputs = IfNode->A.Expression->GetOutputs();
      if (Outputs.Num() > 0) {
        AName = Outputs[IfNode->A.OutputIndex].OutputName.ToString();
      }
    }

    FString BName;
    Parameter = Cast<UMaterialExpressionParameter>(IfNode->B.Expression);
    if (Parameter) {
      BName = Parameter->GetParameterName().ToString();
    } else if (IfNode->B.Expression) {
      TArray<FExpressionOutput>& Outputs = IfNode->B.Expression->GetOutputs();
      if (Outputs.Num() > 0) {
        BName = Outputs[IfNode->B.OutputIndex].OutputName.ToString();
      }
    }
    IfNodeName = AName + BName;

    FString Key = IfNode->GetDescription() + IfNodeName;
    TArray<FExpressionInput*>* pConnections = ConnectionOutputRemap.Find(Key);
    if (pConnections) {
      for (FExpressionInput* pConnection : *pConnections) {
        pConnection->Expression = IfNode;
        pConnection->OutputIndex = 0;
      }
    }

    if (AName.Contains(MaterialPropertyHasValueSuffix)) {
      // Skip the if statement that handles omitted properties. All connections
      // to this node are meant to be autogenerated.
      continue;
    }

    bool isNoDataIfStatement = BName.Contains("NoData");

    TMap<FString, const FExpressionInput*>* pInputConnections =
        ConnectionInputRemap.Find(Key);
    if (pInputConnections) {
      const FExpressionInput** ppAGreaterThanB =
          pInputConnections->Find(TEXT("AGreaterThanB"));
      if (ppAGreaterThanB && *ppAGreaterThanB) {
        IfNode->AGreaterThanB = **ppAGreaterThanB;
      }

      const FExpressionInput** ppALessThanB =
          pInputConnections->Find(TEXT("ALessThanB"));
      if (ppALessThanB && *ppALessThanB) {
        IfNode->ALessThanB = **ppALessThanB;
      }

      if (isNoDataIfStatement && IfNode->AEqualsB.Expression) {
        // If this node is comparing the "no data" value, the property may also
        // have a default value. If it does, it will have already been connected
        // to this expression; don't overwrite it.
        continue;
      }

      const FExpressionInput** ppAEqualsB =
          pInputConnections->Find(TEXT("AEqualsB"));
      if (ppAEqualsB && *ppAEqualsB) {
        IfNode->AEqualsB = **ppAEqualsB;
      }
    }
  }
}

// Increment constant that is used to space out the autogenerated nodes.
static const int32 Incr = 200;

namespace {
ECustomMaterialOutputType
GetOutputTypeForEncodedType(ECesiumEncodedMetadataType Type) {
  switch (Type) {
  case ECesiumEncodedMetadataType::Vec2:
    return ECustomMaterialOutputType::CMOT_Float2;
  case ECesiumEncodedMetadataType::Vec3:
    return ECustomMaterialOutputType::CMOT_Float3;
  case ECesiumEncodedMetadataType::Vec4:
    return ECustomMaterialOutputType::CMOT_Float4;
  case ECesiumEncodedMetadataType::Scalar:
  default:
    return ECustomMaterialOutputType::CMOT_Float1;
  };
}

FString GetSwizzleForEncodedType(ECesiumEncodedMetadataType Type) {
  switch (Type) {
  case ECesiumEncodedMetadataType::Scalar:
    return ".r";
  case ECesiumEncodedMetadataType::Vec2:
    return ".rg";
  case ECesiumEncodedMetadataType::Vec3:
    return ".rgb";
  case ECesiumEncodedMetadataType::Vec4:
    return ".rgba";
  default:
    return FString();
  };
}

UMaterialExpressionMaterialFunctionCall* GenerateNodesForFeatureIdTexture(
    const FCesiumFeatureIdSetDescription& Description,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    UMaterialFunctionMaterialLayer* TargetMaterialLayer,
    UMaterialFunction* GetFeatureIdsFromTextureFunction,
    int32& NodeX,
    int32& NodeY) {
  UMaterialExpressionScalarParameter* TexCoordsIndex =
      NewObject<UMaterialExpressionScalarParameter>(TargetMaterialLayer);
  TexCoordsIndex->ParameterName =
      FName(Description.Name + MaterialTexCoordIndexSuffix);
  TexCoordsIndex->DefaultValue = 0.0f;
  TexCoordsIndex->MaterialExpressionEditorX = NodeX;
  TexCoordsIndex->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(TexCoordsIndex);

  NodeY += Incr;

  UMaterialExpressionTextureObjectParameter* FeatureIdTexture =
      NewObject<UMaterialExpressionTextureObjectParameter>(TargetMaterialLayer);
  FeatureIdTexture->ParameterName =
      FName(Description.Name + MaterialTextureSuffix);
  FeatureIdTexture->MaterialExpressionEditorX = NodeX;
  FeatureIdTexture->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(FeatureIdTexture);

  NodeY += Incr;

  UMaterialExpressionVectorParameter* Channels =
      NewObject<UMaterialExpressionVectorParameter>(TargetMaterialLayer);
  Channels->ParameterName = FName(Description.Name + MaterialChannelsSuffix);
  Channels->DefaultValue = FLinearColor(0, 0, 0, 0);
  Channels->MaterialExpressionEditorX = NodeX;
  Channels->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(Channels);

  NodeY += Incr;

  UMaterialExpressionScalarParameter* NumChannels =
      NewObject<UMaterialExpressionScalarParameter>(TargetMaterialLayer);
  NumChannels->ParameterName =
      FName(Description.Name + MaterialNumChannelsSuffix);
  NumChannels->DefaultValue = 0.0f;
  NumChannels->MaterialExpressionEditorX = NodeX;
  NumChannels->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(NumChannels);

  NodeY -= 1.75 * Incr;
  NodeX += 2 * Incr;

  UMaterialExpressionMaterialFunctionCall* GetFeatureIdsFromTexture =
      NewObject<UMaterialExpressionMaterialFunctionCall>(TargetMaterialLayer);
  GetFeatureIdsFromTexture->MaterialFunction = GetFeatureIdsFromTextureFunction;
  GetFeatureIdsFromTexture->MaterialExpressionEditorX = NodeX;
  GetFeatureIdsFromTexture->MaterialExpressionEditorY = NodeY;

  GetFeatureIdsFromTextureFunction->GetInputsAndOutputs(
      GetFeatureIdsFromTexture->FunctionInputs,
      GetFeatureIdsFromTexture->FunctionOutputs);
  GetFeatureIdsFromTexture->FunctionInputs[0].Input.Expression = TexCoordsIndex;
  GetFeatureIdsFromTexture->FunctionInputs[1].Input.Expression =
      FeatureIdTexture;
  GetFeatureIdsFromTexture->FunctionInputs[2].Input.Expression = Channels;
  GetFeatureIdsFromTexture->FunctionInputs[3].Input.Expression = NumChannels;
  AutoGeneratedNodes.Add(GetFeatureIdsFromTexture);

  return GetFeatureIdsFromTexture;
}

UMaterialExpressionMaterialFunctionCall* GenerateNodesForFeatureIdAttribute(
    const FCesiumFeatureIdSetDescription& Description,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    UMaterialFunctionMaterialLayer* TargetMaterialLayer,
    UMaterialFunction* GetFeatureIdsFromAttributeFunction,
    int32& NodeX,
    int32& NodeY) {
  UMaterialExpressionScalarParameter* TextureCoordinateIndex =
      NewObject<UMaterialExpressionScalarParameter>(TargetMaterialLayer);
  TextureCoordinateIndex->ParameterName = FName(Description.Name);
  TextureCoordinateIndex->DefaultValue = 0.0f;
  TextureCoordinateIndex->MaterialExpressionEditorX = NodeX;
  TextureCoordinateIndex->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(TextureCoordinateIndex);

  NodeX += 2 * Incr;

  UMaterialExpressionMaterialFunctionCall* GetFeatureIdsFromAttribute =
      NewObject<UMaterialExpressionMaterialFunctionCall>(TargetMaterialLayer);
  GetFeatureIdsFromAttribute->MaterialFunction =
      GetFeatureIdsFromAttributeFunction;
  GetFeatureIdsFromAttribute->MaterialExpressionEditorX = NodeX;
  GetFeatureIdsFromAttribute->MaterialExpressionEditorY = NodeY;

  GetFeatureIdsFromAttributeFunction->GetInputsAndOutputs(
      GetFeatureIdsFromAttribute->FunctionInputs,
      GetFeatureIdsFromAttribute->FunctionOutputs);
  GetFeatureIdsFromAttribute->FunctionInputs[0].Input.Expression =
      TextureCoordinateIndex;
  AutoGeneratedNodes.Add(GetFeatureIdsFromAttribute);

  return GetFeatureIdsFromAttribute;
}

void GenerateNodesForNullFeatureId(
    const FCesiumFeatureIdSetDescription& Description,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    UMaterialFunctionMaterialLayer* TargetMaterialLayer,
    int32& NodeX,
    int32& NodeY,
    UMaterialExpression* LastNode) {
  int32 SectionTop = NodeY;
  NodeY += 0.5 * Incr;

  UMaterialExpressionScalarParameter* NullFeatureId =
      NewObject<UMaterialExpressionScalarParameter>(TargetMaterialLayer);
  NullFeatureId->ParameterName =
      FName(Description.Name + MaterialNullFeatureIdSuffix);
  NullFeatureId->DefaultValue = 0;
  NullFeatureId->MaterialExpressionEditorX = NodeX;
  NullFeatureId->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(NullFeatureId);

  NodeY = SectionTop;
  NodeX += Incr * 2;

  UMaterialExpressionIf* IfStatement =
      NewObject<UMaterialExpressionIf>(TargetMaterialLayer);

  IfStatement->A.Expression = LastNode;
  IfStatement->B.Expression = NullFeatureId;

  IfStatement->MaterialExpressionEditorX = NodeX;
  IfStatement->MaterialExpressionEditorY = NodeY;

  AutoGeneratedNodes.Add(IfStatement);
}

UMaterialExpressionParameter* GenerateParameterNodeWithGivenType(
    const ECesiumEncodedMetadataType Type,
    const FString& Name,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    UMaterialFunctionMaterialLayer* TargetMaterialLayer,
    int32& NodeX,
    int32& NodeY) {
  UMaterialExpressionParameter* Parameter = nullptr;
  if (Type == ECesiumEncodedMetadataType::Scalar) {
    UMaterialExpressionScalarParameter* ScalarParameter =
        NewObject<UMaterialExpressionScalarParameter>(TargetMaterialLayer);
    ScalarParameter->DefaultValue = 0.0f;
    Parameter = ScalarParameter;
  }

  if (Type == ECesiumEncodedMetadataType::Vec2 ||
      Type == ECesiumEncodedMetadataType::Vec3 ||
      Type == ECesiumEncodedMetadataType::Vec4) {
    UMaterialExpressionVectorParameter* VectorParameter =
        NewObject<UMaterialExpressionVectorParameter>(TargetMaterialLayer);
    VectorParameter->DefaultValue = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
    Parameter = VectorParameter;
  }

  if (!Parameter) {
    return nullptr;
  }

  Parameter->ParameterName = FName(Name);
  Parameter->MaterialExpressionEditorX = NodeX;
  Parameter->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(Parameter);

  return Parameter;
}

void GenerateNodesForMetadataPropertyTransforms(
    const FCesiumMetadataPropertyDescription& Property,
    const FString& PropertyName,
    const FString& FullPropertyName,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    UMaterialFunctionMaterialLayer* TargetMaterialLayer,
    int32& NodeX,
    int32& NodeY,
    UMaterialExpressionCustom* GetPropertyValuesFunction,
    int32 GetPropertyValuesOutputIndex) {
  int32 SectionLeft = NodeX;
  int32 SectionTop = NodeY;

  UMaterialExpressionCustom* ApplyTransformsFunction = nullptr;
  UMaterialExpressionCustom* GetNoDataValueFunction = nullptr;
  UMaterialExpressionCustom* GetDefaultValueFunction = nullptr;
  UMaterialExpressionIf* NoDataIfNode = nullptr;

  if (Property.PropertyDetails.bIsNormalized ||
      Property.PropertyDetails.bHasScale ||
      Property.PropertyDetails.bHasOffset) {

    ApplyTransformsFunction =
        NewObject<UMaterialExpressionCustom>(TargetMaterialLayer);
    ApplyTransformsFunction->Code = "";
    ApplyTransformsFunction->Description =
        "Apply Value Transforms To " + PropertyName;
    ApplyTransformsFunction->MaterialExpressionEditorX = SectionLeft + 2 * Incr;
    ApplyTransformsFunction->MaterialExpressionEditorY = NodeY;

    ApplyTransformsFunction->Inputs.Reserve(3);
    ApplyTransformsFunction->Outputs.Reset(2);
    ApplyTransformsFunction->AdditionalOutputs.Reserve(1);
    ApplyTransformsFunction->Outputs.Add(FExpressionOutput(FName("Raw Value")));
    ApplyTransformsFunction->bShowOutputNameOnPin = true;
    AutoGeneratedNodes.Add(ApplyTransformsFunction);

    FCustomInput& RawValueInput = ApplyTransformsFunction->Inputs[0];
    RawValueInput.InputName = FName("RawValue");
    RawValueInput.Input.Expression = GetPropertyValuesFunction;
    RawValueInput.Input.OutputIndex = GetPropertyValuesOutputIndex;

    FCustomOutput& TransformedOutput =
        ApplyTransformsFunction->AdditionalOutputs.Emplace_GetRef();
    TransformedOutput.OutputName = FName("TransformedValue");
    ApplyTransformsFunction->Outputs.Add(
        FExpressionOutput(TransformedOutput.OutputName));

    TransformedOutput.OutputType =
        GetOutputTypeForEncodedType(Property.EncodingDetails.Type);

    FString TransformCode = "TransformedValue = ";

    if (Property.PropertyDetails.bIsNormalized) {
      // The normalization can be hardcoded since only normalized uint8s are
      // supported.
      TransformCode += "(RawValue / 255.0f)";
    } else {
      TransformCode += "RawValue";
    }

    if (Property.PropertyDetails.bHasScale) {
      NodeY += 0.75 * Incr;

      UMaterialExpressionParameter* Parameter =
          GenerateParameterNodeWithGivenType(
              Property.EncodingDetails.Type,
              FullPropertyName + MaterialPropertyScaleSuffix,
              AutoGeneratedNodes,
              TargetMaterialLayer,
              NodeX,
              NodeY);

      FString ScaleName = "Scale";

      FCustomInput& DefaultInput =
          ApplyTransformsFunction->Inputs.Emplace_GetRef();
      DefaultInput.InputName = FName(ScaleName);
      DefaultInput.Input.Expression = Parameter;

      TransformCode += " * " + ScaleName;
    }

    if (Property.PropertyDetails.bHasOffset) {
      NodeY += 0.75 * Incr;

      UMaterialExpressionParameter* Parameter =
          GenerateParameterNodeWithGivenType(
              Property.EncodingDetails.Type,
              FullPropertyName + MaterialPropertyOffsetSuffix,
              AutoGeneratedNodes,
              TargetMaterialLayer,
              NodeX,
              NodeY);

      FString OffsetName = "Offset";

      FCustomInput& DefaultInput =
          ApplyTransformsFunction->Inputs.Emplace_GetRef();
      DefaultInput.InputName = FName(OffsetName);
      DefaultInput.Input.Expression = Parameter;

      TransformCode += " + " + OffsetName;
    }

    NodeY += Incr;

    // Example: TransformedValue = (RawValue / 255.0f) * Scale_VALUE +
    // Offset_VALUE;
    ApplyTransformsFunction->Code += TransformCode + ";\n";

    // Return the raw value.
    ApplyTransformsFunction->OutputType =
        GetOutputTypeForEncodedType(Property.EncodingDetails.Type);
    ApplyTransformsFunction->Code += "return RawValue;";
  }

  FString swizzle = GetSwizzleForEncodedType(Property.EncodingDetails.Type);

  if (Property.PropertyDetails.bHasNoDataValue) {
    UMaterialExpressionParameter* Parameter =
        GenerateParameterNodeWithGivenType(
            Property.EncodingDetails.Type,
            FullPropertyName + MaterialPropertyNoDataSuffix,
            AutoGeneratedNodes,
            TargetMaterialLayer,
            NodeX,
            NodeY);

    // This is equivalent to a "MakeFloatN" function.
    GetNoDataValueFunction =
        NewObject<UMaterialExpressionCustom>(TargetMaterialLayer);
    GetNoDataValueFunction->Description =
        "Get No Data Value For " + PropertyName;
    GetNoDataValueFunction->MaterialExpressionEditorX = SectionLeft + 2 * Incr;
    GetNoDataValueFunction->MaterialExpressionEditorY = NodeY;

    GetNoDataValueFunction->Outputs.Reset(1);
    GetNoDataValueFunction->bShowOutputNameOnPin = true;
    AutoGeneratedNodes.Add(GetNoDataValueFunction);

    FString NoDataName = "NoData";
    FString InputName = NoDataName + MaterialPropertyValueSuffix;

    FCustomInput& NoDataInput = GetNoDataValueFunction->Inputs[0];
    NoDataInput.InputName = FName(InputName);
    NoDataInput.Input.Expression = Parameter;

    GetNoDataValueFunction->Outputs.Add(FExpressionOutput(FName(NoDataName)));
    GetNoDataValueFunction->OutputType =
        GetOutputTypeForEncodedType(Property.EncodingDetails.Type);

    GetNoDataValueFunction->Code = "return " + InputName + swizzle + ";\n";
    NodeY += Incr;
  }

  if (Property.PropertyDetails.bHasDefaultValue) {
    UMaterialExpressionParameter* Parameter =
        GenerateParameterNodeWithGivenType(
            Property.EncodingDetails.Type,
            FullPropertyName + MaterialPropertyDefaultValueSuffix,
            AutoGeneratedNodes,
            TargetMaterialLayer,
            NodeX,
            NodeY);

    // This is equivalent to a "MakeFloatN" function.
    GetDefaultValueFunction =
        NewObject<UMaterialExpressionCustom>(TargetMaterialLayer);
    GetDefaultValueFunction->Description =
        "Get Default Value For " + PropertyName;
    GetDefaultValueFunction->MaterialExpressionEditorX = SectionLeft + 2 * Incr;
    GetDefaultValueFunction->MaterialExpressionEditorY = NodeY;

    GetDefaultValueFunction->Outputs.Reset(1);
    GetDefaultValueFunction->bShowOutputNameOnPin = true;
    AutoGeneratedNodes.Add(GetDefaultValueFunction);

    FString DefaultName = "Default";
    FString InputName = DefaultName + MaterialPropertyValueSuffix;

    FCustomInput& DefaultInput = GetDefaultValueFunction->Inputs[0];
    DefaultInput.InputName = FName(InputName);
    DefaultInput.Input.Expression = Parameter;

    GetDefaultValueFunction->Outputs.Add(
        FExpressionOutput(FName("Default Value")));
    GetDefaultValueFunction->OutputType =
        GetOutputTypeForEncodedType(Property.EncodingDetails.Type);

    // Example: Default = Default_VALUE.xyz;
    GetDefaultValueFunction->Code = "return " + InputName + swizzle + ";\n";
    NodeY += Incr;
  }

  NodeX += 3.5 * Incr;

  // We want to return to the top of the section and work down again, without
  // overwriting NodeY. At the end, we use the maximum value to determine the
  // vertical extent of the entire section.
  int32_t SectionNodeY = SectionTop;

  // Add if statement for resolving the no data / default values
  if (GetNoDataValueFunction) {
    NodeX += Incr;

    NoDataIfNode = NewObject<UMaterialExpressionIf>(TargetMaterialLayer);
    NoDataIfNode->MaterialExpressionEditorX = NodeX;
    NoDataIfNode->MaterialExpressionEditorY = SectionNodeY;

    NoDataIfNode->B.Expression = GetNoDataValueFunction;
    NoDataIfNode->AEqualsB.Expression = GetDefaultValueFunction;

    if (ApplyTransformsFunction) {
      NoDataIfNode->A.Expression = ApplyTransformsFunction;
      NoDataIfNode->A.OutputIndex = 0;

      NoDataIfNode->AGreaterThanB.Expression = ApplyTransformsFunction;
      NoDataIfNode->AGreaterThanB.OutputIndex = 1;

      NoDataIfNode->ALessThanB.Expression = ApplyTransformsFunction;
      NoDataIfNode->ALessThanB.OutputIndex = 1;
    } else {
      NoDataIfNode->A.Expression = GetPropertyValuesFunction;
      NoDataIfNode->A.OutputIndex = GetPropertyValuesOutputIndex;

      NoDataIfNode->AGreaterThanB.Expression = GetPropertyValuesFunction;
      NoDataIfNode->AGreaterThanB.OutputIndex = GetPropertyValuesOutputIndex;

      NoDataIfNode->ALessThanB.Expression = GetPropertyValuesFunction;
      NoDataIfNode->ALessThanB.OutputIndex = GetPropertyValuesOutputIndex;
    }

    AutoGeneratedNodes.Add(NoDataIfNode);
    NodeX += 2 * Incr;
  }

  // If the property has a default value defined, it may be omitted from an
  // instance of a property table, texture, or attribute. In this case, the
  // default value should be used without needing to execute the
  // GetPropertyValues function. We check this with a scalar parameter that
  // acts as a boolean.
  if (GetDefaultValueFunction) {
    UMaterialExpressionScalarParameter* HasValueParameter =
        NewObject<UMaterialExpressionScalarParameter>(TargetMaterialLayer);
    HasValueParameter->DefaultValue = 0.0f;
    HasValueParameter->ParameterName =
        FName(FullPropertyName + MaterialPropertyHasValueSuffix);
    HasValueParameter->MaterialExpressionEditorX = NodeX;
    HasValueParameter->MaterialExpressionEditorY = SectionNodeY;
    AutoGeneratedNodes.Add(HasValueParameter);

    NodeX += 2 * Incr;
    UMaterialExpressionIf* IfStatement =
        NewObject<UMaterialExpressionIf>(TargetMaterialLayer);
    IfStatement->MaterialExpressionEditorX = NodeX;
    IfStatement->MaterialExpressionEditorY = SectionNodeY;

    IfStatement->A.Expression = HasValueParameter;
    IfStatement->ConstB = 1.0f;

    IfStatement->ALessThanB.Expression = GetDefaultValueFunction;

    if (NoDataIfNode) {
      IfStatement->AGreaterThanB.Expression = NoDataIfNode;
      IfStatement->AEqualsB.Expression = NoDataIfNode;
    } else if (ApplyTransformsFunction) {
      IfStatement->AGreaterThanB.Expression = ApplyTransformsFunction;
      IfStatement->AGreaterThanB.OutputIndex = 1;

      IfStatement->AEqualsB.Expression = ApplyTransformsFunction;
      IfStatement->AEqualsB.OutputIndex = 1;
    } else {
      IfStatement->AGreaterThanB.Expression = GetPropertyValuesFunction;
      IfStatement->AGreaterThanB.OutputIndex = GetPropertyValuesOutputIndex;

      IfStatement->AEqualsB.Expression = GetPropertyValuesFunction;
      IfStatement->AEqualsB.OutputIndex = GetPropertyValuesOutputIndex;
    }

    AutoGeneratedNodes.Add(IfStatement);
  }

  if (SectionNodeY > NodeY) {
    NodeY = SectionNodeY;
  }
}

void GenerateNodesForPropertyTable(
    const FCesiumPropertyTableDescription& PropertyTable,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    UMaterialFunctionMaterialLayer* TargetMaterialLayer,
    int32& NodeX,
    int32& NodeY,
    UMaterialExpressionMaterialFunctionCall* GetFeatureIdCall) {
  int32 SectionLeft = NodeX;
  int32 PropertyDataSectionY = NodeY - 0.5 * Incr;
  int32 PropertyTransformsSectionY = NodeY + 20;

  UMaterialExpressionCustom* GetPropertyValuesFunction =
      NewObject<UMaterialExpressionCustom>(TargetMaterialLayer);
  GetPropertyValuesFunction->Inputs.Reserve(PropertyTable.Properties.Num() + 2);
  GetPropertyValuesFunction->Outputs.Reset(PropertyTable.Properties.Num() + 1);
  GetPropertyValuesFunction->Outputs.Add(FExpressionOutput(TEXT("Feature ID")));
  GetPropertyValuesFunction->bShowOutputNameOnPin = true;
  GetPropertyValuesFunction->Code = "";
  GetPropertyValuesFunction->Description =
      "Get Property Values From " + PropertyTable.Name;
  GetPropertyValuesFunction->MaterialExpressionEditorX = SectionLeft + 2 * Incr;
  GetPropertyValuesFunction->MaterialExpressionEditorY = NodeY;
  AutoGeneratedNodes.Add(GetPropertyValuesFunction);

  FCustomInput& FeatureIDInput = GetPropertyValuesFunction->Inputs[0];
  FeatureIDInput.InputName = FName("FeatureID");
  FeatureIDInput.Input.Expression = GetFeatureIdCall;

  GetPropertyValuesFunction->AdditionalOutputs.Reserve(
      PropertyTable.Properties.Num());

  bool foundFirstProperty = false;
  for (const FCesiumMetadataPropertyDescription& property :
       PropertyTable.Properties) {
    if (property.EncodingDetails.Conversion ==
            ECesiumEncodedMetadataConversion::None ||
        !property.EncodingDetails.HasValidType()) {
      continue;
    }

    PropertyDataSectionY += Incr;

    FString propertyName = createHlslSafeName(property.Name);
    // Example: "roofColor_DATA"
    FString PropertyDataName = propertyName + MaterialPropertyDataSuffix;

    if (!foundFirstProperty) {
      // Get the dimensions of the first valid property. All the properties
      // will have the same pixel dimensions since it is based on the feature
      // count.
      GetPropertyValuesFunction->Code +=
          "uint _czm_width;\nuint _czm_height;\n";
      GetPropertyValuesFunction->Code +=
          PropertyDataName + ".GetDimensions(_czm_width, _czm_height);\n";
      GetPropertyValuesFunction->Code +=
          "uint _czm_pixelX = FeatureID % _czm_width;\n";
      GetPropertyValuesFunction->Code +=
          "uint _czm_pixelY = FeatureID / _czm_width;\n";

      foundFirstProperty = true;
    }

    UMaterialExpressionTextureObjectParameter* PropertyData =
        NewObject<UMaterialExpressionTextureObjectParameter>(
            TargetMaterialLayer);
    FString FullPropertyName = getMaterialNameForPropertyTableProperty(
        PropertyTable.Name,
        propertyName);
    PropertyData->ParameterName = FName(FullPropertyName);
    PropertyData->MaterialExpressionEditorX = SectionLeft;
    PropertyData->MaterialExpressionEditorY = PropertyDataSectionY;
    AutoGeneratedNodes.Add(PropertyData);

    FCustomInput& PropertyInput =
        GetPropertyValuesFunction->Inputs.Emplace_GetRef();
    PropertyInput.InputName = FName(PropertyDataName);
    PropertyInput.Input.Expression = PropertyData;

    FCustomOutput& PropertyOutput =
        GetPropertyValuesFunction->AdditionalOutputs.Emplace_GetRef();

    FString OutputName = propertyName;
    if (property.PropertyDetails.bIsNormalized ||
        property.PropertyDetails.bHasOffset ||
        property.PropertyDetails.bHasScale) {
      OutputName += MaterialPropertyRawSuffix;
    }

    PropertyOutput.OutputName = FName(OutputName);
    GetPropertyValuesFunction->Outputs.Add(
        FExpressionOutput(PropertyOutput.OutputName));

    FString swizzle = GetSwizzleForEncodedType(property.EncodingDetails.Type);
    PropertyOutput.OutputType =
        GetOutputTypeForEncodedType(property.EncodingDetails.Type);

    FString asComponentString =
        property.EncodingDetails.ComponentType ==
                ECesiumEncodedMetadataComponentType::Float
            ? "asfloat"
            : "asuint";

    // Example:
    // "color = asfloat(color_DATA.Load(int3(_czm_pixelX, _czm_pixelY,
    // 0)).rgb);"
    GetPropertyValuesFunction->Code +=
        OutputName + " = " + asComponentString + "(" + PropertyDataName +
        ".Load(int3(_czm_pixelX, _czm_pixelY, 0))" + swizzle + ");\n";

    if (property.PropertyDetails.HasValueTransforms()) {
      int32 PropertyTransformsSectionX = SectionLeft + 4 * Incr;
      GenerateNodesForMetadataPropertyTransforms(
          property,
          propertyName,
          FullPropertyName,
          AutoGeneratedNodes,
          TargetMaterialLayer,
          PropertyTransformsSectionX,
          PropertyTransformsSectionY,
          GetPropertyValuesFunction,
          GetPropertyValuesFunction->Outputs.Num() - 1);

      NodeX = FMath::Max(NodeX, PropertyTransformsSectionX);
    }
  }

  // Return the feature ID.
  GetPropertyValuesFunction->OutputType =
      ECustomMaterialOutputType::CMOT_Float1;
  GetPropertyValuesFunction->Code += "return FeatureID;";

  NodeY = FMath::Max(PropertyDataSectionY, PropertyTransformsSectionY);
}

void GenerateNodesForPropertyTexture(
    // const TArray<FCesiumPropertyTableDescription>& Descriptions,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    UMaterialFunctionMaterialLayer* TargetMaterialLayer,
    int32& NodeX,
    int32& NodeY) {
  //  for (const FFeatureTextureDescription& featureTexture :
  //       this->FeatureTextures) {
  //    int32 SectionLeft = NodeX;
  //    int32 SectionTop = NodeY;
  //
  //    UMaterialExpressionCustom* FeatureTextureLookup =
  //        NewObject<UMaterialExpressionCustom>(this->TargetMaterialLayer);
  //    FeatureTextureLookup->Inputs.Reset(2 *
  //    featureTexture.Properties.Num());
  //    FeatureTextureLookup->Outputs.Reset(featureTexture.Properties.Num() +
  //    1);
  //    FeatureTextureLookup->Outputs.Add(FExpressionOutput(TEXT("return")));
  //    FeatureTextureLookup->bShowOutputNameOnPin = true;
  //    FeatureTextureLookup->Code = "";
  //    FeatureTextureLookup->Description =
  //        "Resolve properties from " + featureTexture.Name;
  //    FeatureTextureLookup->MaterialExpressionEditorX = NodeX + 2 * IncrX;
  //    FeatureTextureLookup->MaterialExpressionEditorY = NodeY;
  //    AutoGeneratedNodes.Add(FeatureTextureLookup);
  //
  //    for (const FFeatureTexturePropertyDescription& property :
  //         featureTexture.Properties) {
  //      UMaterialExpressionTextureObjectParameter* PropertyTexture =
  //          NewObject<UMaterialExpressionTextureObjectParameter>(
  //              this->TargetMaterialLayer);
  //      PropertyTexture->ParameterName =
  //          FName("FTX_" + featureTexture.Name + "_" + property.Name +
  //          "_TX");
  //      PropertyTexture->MaterialExpressionEditorX = NodeX;
  //      PropertyTexture->MaterialExpressionEditorY = NodeY;
  //      AutoGeneratedNodes.Add(PropertyTexture);
  //
  //      FString propertyName = createHlslSafeName(property.Name);
  //
  //      FCustomInput& PropertyTextureInput =
  //          FeatureTextureLookup->Inputs.Emplace_GetRef();
  //      FString propertyTextureName = propertyName + "_TX";
  //      PropertyTextureInput.InputName = FName(propertyTextureName);
  //      PropertyTextureInput.Input.Expression = PropertyTexture;
  //
  //      NodeY += IncrY;
  //
  //      UMaterialExpressionScalarParameter* TexCoordsIndex =
  //          NewObject<UMaterialExpressionScalarParameter>(
  //              this->TargetMaterialLayer);
  //      TexCoordsIndex->ParameterName =
  //          FName("FTX_" + featureTexture.Name + "_" + property.Name +
  //          "_UV");
  //      TexCoordsIndex->DefaultValue = 0.0f;
  //      TexCoordsIndex->MaterialExpressionEditorX = NodeX;
  //      TexCoordsIndex->MaterialExpressionEditorY = NodeY;
  //      AutoGeneratedNodes.Add(TexCoordsIndex);
  //
  //      NodeX += IncrX;
  //
  //      UMaterialExpressionMaterialFunctionCall* SelectTexCoords =
  //          NewObject<UMaterialExpressionMaterialFunctionCall>(
  //              this->TargetMaterialLayer);
  //      SelectTexCoords->MaterialFunction = SelectTexCoordsFunction;
  //      SelectTexCoords->MaterialExpressionEditorX = NodeX;
  //      SelectTexCoords->MaterialExpressionEditorY = NodeY;
  //
  //      SelectTexCoordsFunction->GetInputsAndOutputs(
  //          SelectTexCoords->FunctionInputs,
  //          SelectTexCoords->FunctionOutputs);
  //      SelectTexCoords->FunctionInputs[0].Input.Expression =
  //      TexCoordsIndex; AutoGeneratedNodes.Add(SelectTexCoords);
  //
  //      FCustomInput& TexCoordsInput =
  //          FeatureTextureLookup->Inputs.Emplace_GetRef();
  //      FString propertyUvName = propertyName + "_UV";
  //      TexCoordsInput.InputName = FName(propertyUvName);
  //      TexCoordsInput.Input.Expression = SelectTexCoords;
  //
  //      FCustomOutput& PropertyOutput =
  //          FeatureTextureLookup->AdditionalOutputs.Emplace_GetRef();
  //      PropertyOutput.OutputName = FName(propertyName);
  //      FeatureTextureLookup->Outputs.Add(
  //          FExpressionOutput(PropertyOutput.OutputName));
  //
  //      // Either the property is normalized or it is coerced into float.
  //      Either
  //      // way, the outputs will be float type.
  //      switch (property.Type) {
  //      case ECesiumPropertyType::Vec2:
  //        PropertyOutput.OutputType =
  //        ECustomMaterialOutputType::CMOT_Float2; break;
  //      case ECesiumPropertyType::Vec3:
  //        PropertyOutput.OutputType =
  //        ECustomMaterialOutputType::CMOT_Float3; break;
  //      case ECesiumPropertyType::Vec4:
  //        PropertyOutput.OutputType =
  //        ECustomMaterialOutputType::CMOT_Float4; break;
  //      // case ECesiumPropertyType::Scalar:
  //      default:
  //        PropertyOutput.OutputType =
  //        ECustomMaterialOutputType::CMOT_Float1;
  //      };
  //
  //      // TODO: should dynamic channel offsets be used instead of swizzle
  //      string
  //      // determined at editor time? E.g. can swizzles be different for the
  //      same
  //      // property texture on different tiles?
  //      FeatureTextureLookup->Code +=
  //          propertyName + " = " +
  //          (property.Normalized ? "asfloat(" : "asuint(") +
  //          propertyTextureName +
  //          ".Sample(" + propertyTextureName + "Sampler, " + propertyUvName
  //          +
  //          ")." + property.Swizzle + ");\n";
  //
  //      NodeY += IncrY;
  //    }
  //
  //    FeatureTextureLookup->OutputType =
  //    ECustomMaterialOutputType::CMOT_Float1; FeatureTextureLookup->Code +=
  //    "return 0.0f;";
  //
  //    NodeX = SectionLeft;
  //  }
}

void GenerateMaterialNodes(
    UCesiumFeaturesMetadataComponent* pComponent,
    TArray<UMaterialExpression*>& AutoGeneratedNodes,
    TArray<UMaterialExpression*>& OneTimeGeneratedNodes,
    UMaterialFunction* GetFeatureIdsFromAttributeFunction,
    UMaterialFunction* GetFeatureIdsFromTextureFunction) {
  int32 NodeX = 0;
  int32 NodeY = 0;

  TSet<FString> GeneratedPropertyTableNames;

  int32 FeatureIdSectionLeft = NodeX;
  int32 PropertyTableSectionLeft = FeatureIdSectionLeft + 4 * Incr;
  for (const FCesiumFeatureIdSetDescription& featureIdSet :
       pComponent->FeatureIdSets) {
    if (featureIdSet.Type == ECesiumFeatureIdSetType::None) {
      continue;
    }

    UMaterialExpressionMaterialFunctionCall* GetFeatureIdCall = nullptr;
    if (featureIdSet.Type == ECesiumFeatureIdSetType::Texture) {
      GetFeatureIdCall = GenerateNodesForFeatureIdTexture(
          featureIdSet,
          AutoGeneratedNodes,
          pComponent->TargetMaterialLayer,
          GetFeatureIdsFromTextureFunction,
          NodeX,
          NodeY);
    } else {
      // Handle implicit feature IDs the same as feature ID attributes
      GetFeatureIdCall = GenerateNodesForFeatureIdAttribute(
          featureIdSet,
          AutoGeneratedNodes,
          pComponent->TargetMaterialLayer,
          GetFeatureIdsFromAttributeFunction,
          NodeX,
          NodeY);
    }

    UMaterialExpression* LastNode = GetFeatureIdCall;
    int32 SectionTop = NodeY;

    if (!featureIdSet.PropertyTableName.IsEmpty()) {
      const FCesiumPropertyTableDescription* pPropertyTable =
          pComponent->PropertyTables.FindByPredicate(
              [&name = featureIdSet.PropertyTableName](
                  const FCesiumPropertyTableDescription&
                      existingPropertyTable) {
                return existingPropertyTable.Name == name;
              });

      if (pPropertyTable) {
        NodeX = PropertyTableSectionLeft;
        GenerateNodesForPropertyTable(
            *pPropertyTable,
            AutoGeneratedNodes,
            pComponent->TargetMaterialLayer,
            NodeX,
            NodeY,
            GetFeatureIdCall);
        GeneratedPropertyTableNames.Add(pPropertyTable->Name);
      }
    }

    if (featureIdSet.bHasNullFeatureId) {
      // Spatial nitpicking; this aligns the if statement to the same Y as the
      // PropertyTableFunction node then resets the Y so that the next section
      // appears below all of the just-generated nodes.
      int32 OriginalY = NodeY;

      NodeX += 2 * Incr;
      NodeY = SectionTop;

      GenerateNodesForNullFeatureId(
          featureIdSet,
          AutoGeneratedNodes,
          pComponent->TargetMaterialLayer,
          NodeX,
          NodeY,
          LastNode);

      NodeY = OriginalY;
    }

    NodeX = FeatureIdSectionLeft;
    NodeY += 2 * Incr;
  }

  NodeX = PropertyTableSectionLeft;

  // Generate nodes for any property tables that aren't linked to a feature ID
  // set.
  for (const FCesiumPropertyTableDescription& propertyTable :
       pComponent->PropertyTables) {
    if (!GeneratedPropertyTableNames.Find(propertyTable.Name)) {
      GenerateNodesForPropertyTable(
          propertyTable,
          AutoGeneratedNodes,
          pComponent->TargetMaterialLayer,
          NodeX,
          NodeY,
          nullptr);
      NodeX = PropertyTableSectionLeft;
      NodeY += Incr;
    }
  }

  // GenerateNodesForPropertyTextures

  NodeX = FeatureIdSectionLeft;
  NodeY = -2 * Incr;

  UMaterialExpressionFunctionInput* InputMaterial = nullptr;
#if ENGINE_MINOR_VERSION == 0
  for (UMaterialExpression* ExistingNode :
       pComponent->TargetMaterialLayer->FunctionExpressions) {
#else
  for (const TObjectPtr<UMaterialExpression>& ExistingNode :
       pComponent->TargetMaterialLayer->GetExpressionCollection().Expressions) {
#endif
    UMaterialExpressionFunctionInput* ExistingInputMaterial =
        Cast<UMaterialExpressionFunctionInput>(ExistingNode);
    if (ExistingInputMaterial) {
      InputMaterial = ExistingInputMaterial;
      break;
    }
  }

  if (!InputMaterial) {
    InputMaterial = NewObject<UMaterialExpressionFunctionInput>(
        pComponent->TargetMaterialLayer);
    InputMaterial->InputType =
        EFunctionInputType::FunctionInput_MaterialAttributes;
    InputMaterial->bUsePreviewValueAsDefault = true;
    InputMaterial->MaterialExpressionEditorX = NodeX;
    InputMaterial->MaterialExpressionEditorY = NodeY;
    OneTimeGeneratedNodes.Add(InputMaterial);
  }

  NodeX += PropertyTableSectionLeft + 8 * Incr;

  UMaterialExpressionSetMaterialAttributes* SetMaterialAttributes = nullptr;
#if ENGINE_MINOR_VERSION == 0
  for (UMaterialExpression* ExistingNode :
       pComponent->TargetMaterialLayer->FunctionExpressions) {
#else
  for (const TObjectPtr<UMaterialExpression>& ExistingNode :
       pComponent->TargetMaterialLayer->GetExpressionCollection().Expressions) {
#endif
    UMaterialExpressionSetMaterialAttributes* ExistingSetAttributes =
        Cast<UMaterialExpressionSetMaterialAttributes>(ExistingNode);
    if (ExistingSetAttributes) {
      SetMaterialAttributes = ExistingSetAttributes;
      break;
    }
  }

  if (!SetMaterialAttributes) {
    SetMaterialAttributes = NewObject<UMaterialExpressionSetMaterialAttributes>(
        pComponent->TargetMaterialLayer);
    OneTimeGeneratedNodes.Add(SetMaterialAttributes);
  }

  SetMaterialAttributes->Inputs[0].Expression = InputMaterial;
  SetMaterialAttributes->MaterialExpressionEditorX = NodeX;
  SetMaterialAttributes->MaterialExpressionEditorY = NodeY;

  NodeX += 2 * Incr;

  UMaterialExpressionFunctionOutput* OutputMaterial = nullptr;
#if ENGINE_MINOR_VERSION == 0
  for (UMaterialExpression* ExistingNode :
       pComponent->TargetMaterialLayer->FunctionExpressions) {
#else
  for (const TObjectPtr<UMaterialExpression>& ExistingNode :
       pComponent->TargetMaterialLayer->GetExpressionCollection().Expressions) {
#endif
    UMaterialExpressionFunctionOutput* ExistingOutputMaterial =
        Cast<UMaterialExpressionFunctionOutput>(ExistingNode);
    if (ExistingOutputMaterial) {
      OutputMaterial = ExistingOutputMaterial;
      break;
    }
  }

  if (!OutputMaterial) {
    OutputMaterial = NewObject<UMaterialExpressionFunctionOutput>(
        pComponent->TargetMaterialLayer);
    OneTimeGeneratedNodes.Add(OutputMaterial);
  }

  OutputMaterial->MaterialExpressionEditorX = NodeX;
  OutputMaterial->MaterialExpressionEditorY = NodeY;
  OutputMaterial->A = FMaterialAttributesInput();
  OutputMaterial->A.Expression = SetMaterialAttributes;
}

} // namespace

void UCesiumFeaturesMetadataComponent::GenerateMaterial() {
  ACesium3DTileset* pTileset = Cast<ACesium3DTileset>(this->GetOwner());
  if (!pTileset) {
    return;
  }

  FString MaterialName =
      "ML_" + pTileset->GetFName().ToString() + "_FeaturesMetadata";
  FString PackageBaseName = "/Game/";
  FString PackageName = PackageBaseName + MaterialName;

  // UMaterialFunction* SelectTexCoordsFunction = LoadMaterialFunction(
  //    "/CesiumForUnreal/Materials/MaterialFunctions/CesiumSelectTexCoords.CesiumSelectTexCoords");
  UMaterialFunction* GetFeatureIdsFromAttributeFunction = LoadMaterialFunction(
      "/CesiumForUnreal/Materials/MaterialFunctions/CesiumGetFeatureIdsFromAttribute.CesiumGetFeatureIdsFromAttribute");
  UMaterialFunction* GetFeatureIdsFromTextureFunction = LoadMaterialFunction(
      "/CesiumForUnreal/Materials/MaterialFunctions/CesiumGetFeatureIdsFromTexture.CesiumGetFeatureIdsFromTexture");

  if (!GetFeatureIdsFromAttributeFunction ||
      !GetFeatureIdsFromTextureFunction) {
    UE_LOG(
        LogCesium,
        Error,
        TEXT(
            "Can't find the material functions necessary to generate material. Aborting."));
    return;
  }

  if (this->TargetMaterialLayer &&
      this->TargetMaterialLayer->GetPackage()->IsDirty()) {
    UE_LOG(
        LogCesium,
        Error,
        TEXT(
            "Can't regenerate a material layer that has unsaved changes. Please save your changes and try again."));
    return;
  }

  bool Overwriting = false;
  if (this->TargetMaterialLayer) {
    // Overwriting an existing material layer.
    Overwriting = true;
    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
        ->CloseAllEditorsForAsset(this->TargetMaterialLayer);
  } else {
    UPackage* Package = CreatePackage(*PackageName);

    // Create an Unreal material layer
    UMaterialFunctionMaterialLayerFactory* MaterialFactory =
        NewObject<UMaterialFunctionMaterialLayerFactory>();
    this->TargetMaterialLayer =
        (UMaterialFunctionMaterialLayer*)MaterialFactory->FactoryCreateNew(
            UMaterialFunctionMaterialLayer::StaticClass(),
            Package,
            *MaterialName,
            RF_Public | RF_Standalone | RF_Transactional,
            NULL,
            GWarn);
    FAssetRegistryModule::AssetCreated(this->TargetMaterialLayer);
    Package->FullyLoad();
    Package->SetDirtyFlag(true);
  }

  this->TargetMaterialLayer->PreEditChange(NULL);

  // Maps autogenerated nodes to the FExpressionInputs that it previously sent
  // its outputs to.
  TMap<FString, TArray<FExpressionInput*>> ConnectionOutputRemap;
  // Maps autogenerated nodes to the FExpressionInputs that it previously took
  // inputs from.
  TMap<FString, TMap<FString, const FExpressionInput*>> ConnectionInputRemap;

  ClearAutoGeneratedNodes(
      this->TargetMaterialLayer,
      ConnectionInputRemap,
      ConnectionOutputRemap,
      GetFeatureIdsFromAttributeFunction,
      GetFeatureIdsFromTextureFunction);

  TArray<UMaterialExpression*> AutoGeneratedNodes;
  TArray<UMaterialExpression*> OneTimeGeneratedNodes;

  GenerateMaterialNodes(
      this,
      AutoGeneratedNodes,
      OneTimeGeneratedNodes,
      GetFeatureIdsFromAttributeFunction,
      GetFeatureIdsFromTextureFunction);

  // Add the generated nodes to the material.

  for (UMaterialExpression* AutoGeneratedNode : AutoGeneratedNodes) {
    // Mark as auto-generated. If the material is regenerated, we will look
    // for this exact description to determine whether it was autogenerated.

    AutoGeneratedNode->Desc = "AUTOGENERATED DO NOT EDIT";
#if ENGINE_MINOR_VERSION == 0
    this->TargetMaterialLayer->FunctionExpressions.Add(AutoGeneratedNode);
#else
    this->TargetMaterialLayer->GetExpressionCollection().AddExpression(
        AutoGeneratedNode);
#endif
  }

  for (UMaterialExpression* OneTimeGeneratedNode : OneTimeGeneratedNodes) {
#if ENGINE_MINOR_VERSION == 0
    this->TargetMaterialLayer->FunctionExpressions.Add(OneTimeGeneratedNode);
#else
    this->TargetMaterialLayer->GetExpressionCollection().AddExpression(
        OneTimeGeneratedNode);
#endif
  }

  RemapUserConnections(
      this->TargetMaterialLayer,
      ConnectionInputRemap,
      ConnectionOutputRemap,
      GetFeatureIdsFromAttributeFunction,
      GetFeatureIdsFromTextureFunction);

  // Let the material update itself if necessary
  this->TargetMaterialLayer->PostEditChange();

  // Make sure that any static meshes, etc using this material will stop
  // using the FMaterialResource of the original material, and will use the
  // new FMaterialResource created when we make a new UMaterial in place
  FGlobalComponentReregisterContext RecreateComponents;

  // If this is a new material, open the content browser to the auto-generated
  // material.
  if (!Overwriting) {
    FContentBrowserModule* pContentBrowserModule =
        FModuleManager::Get().GetModulePtr<FContentBrowserModule>(
            "ContentBrowser");
    if (pContentBrowserModule) {
      TArray<UObject*> AssetsToHighlight;
      AssetsToHighlight.Add(this->TargetMaterialLayer);
      pContentBrowserModule->Get().SyncBrowserToAssets(AssetsToHighlight);
    }
  }

  // Open the updated material in editor.
  if (GEditor) {
    UAssetEditorSubsystem* pAssetEditor =
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (pAssetEditor) {
      pAssetEditor->OpenEditorForAsset(this->TargetMaterialLayer);
      IMaterialEditor* pMaterialEditor = static_cast<IMaterialEditor*>(
          pAssetEditor->FindEditorForAsset(this->TargetMaterialLayer, true));
      if (pMaterialEditor) {
        pMaterialEditor->UpdateMaterialAfterGraphChange();
      }
    }
  }
}

#endif // WITH_EDITOR
