// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectExporterBPLibrary.h"
#include "ObjectExporter.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Camera/CameraActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"

#define JSON_FILE_POSTFIX ".json"
#define STATIC_MESH_BINARY_FILE_POSTFIX ".stm"
#define SKELETAL_MESH_BINARY_FILE_POSTFIX ".skm"
#define SKELETON_BINARY_FILE_POSTFIX ".skt"
#define ANIMSEQUENCE_BINARY_FILE_POSTFIX ".anm"
#define MAP_BINARY_FILE_POSTFIX ".map"

DECLARE_LOG_CATEGORY_CLASS(ObjectExporterBPLibraryLog, Log, All);

UObjectExporterBPLibrary::UObjectExporterBPLibrary(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{

}

bool UObjectExporterBPLibrary::ExportStaticMesh(const UStaticMesh* StaticMesh, const FString& FullFilePathName)
{
    FText OutError;
    if (!FFileHelper::IsFilenameValidForSaving(FullFilePathName, OutError))
    {
        UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportStaticMesh: FullFilePathName is not valid. %s"), *OutError.ToString());

        return false;
    }

    if (StaticMesh != nullptr)
    {
        if (FullFilePathName.EndsWith(JSON_FILE_POSTFIX))
        {
            const int32 FileVersion = 1;
            TSharedRef<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);
            JsonRootObject->SetNumberField("FileVersion", FileVersion);
            JsonRootObject->SetStringField("MeshName", *StaticMesh->GetName());

            if (StaticMesh->RenderData != nullptr)
            {
                // Vertex format
                TArray<TSharedPtr<FJsonValue>> JsonVertexFormat;
                JsonRootObject->SetArrayField("VertexFormat", JsonVertexFormat);

                // LODs
                JsonRootObject->SetNumberField("LODCount", StaticMesh->RenderData->LODResources.Num());

                int32 LODIndex = 0;
                TArray< TSharedPtr<FJsonValue> > JsonLODDatas;
                for (const FStaticMeshLODResources& CurLOD : StaticMesh->RenderData->LODResources)
                {
                    TSharedRef<FJsonObject> JsonLODSingle = MakeShareable(new FJsonObject);
                    JsonLODSingle->SetNumberField("LOD", LODIndex);

                    // Vertex data
                    TArray<TSharedPtr<FJsonValue>> JsonVertices;
                    const FPositionVertexBuffer& VertexBuffer = CurLOD.VertexBuffers.PositionVertexBuffer;

                    JsonLODSingle->SetNumberField("VertexCount", VertexBuffer.GetNumVertices());

                    for (uint32 iVertex = 0; iVertex < VertexBuffer.GetNumVertices(); iVertex++)
                    {
                        const FVector& Position = VertexBuffer.VertexPosition(iVertex);

                        TSharedRef<FJsonObject> JsonVertex = MakeShareable(new FJsonObject);
                        JsonVertex->SetNumberField("x", Position.X);
                        JsonVertex->SetNumberField("y", Position.Y);
                        JsonVertex->SetNumberField("z", Position.Z);

                        JsonVertices.Emplace(MakeShareable(new FJsonValueObject(JsonVertex)));
                    }
                    JsonLODSingle->SetArrayField("Vertices", JsonVertices);

                    // Index data
                    TArray<TSharedPtr<FJsonValue>> JsonIndices;
                    FIndexArrayView Indices = CurLOD.IndexBuffer.GetArrayView();

                    JsonLODSingle->SetNumberField("IndexCount", Indices.Num());

                    for (int32 iIndex = 0; iIndex < Indices.Num(); iIndex++)
                    {
                        TSharedRef<FJsonObject> JsonIndex = MakeShareable(new FJsonObject);
                        JsonIndex->SetNumberField("index", Indices[iIndex]);

                        JsonIndices.Emplace(MakeShareable(new FJsonValueObject(JsonIndex)));
                    }
                    JsonLODSingle->SetArrayField("Indices", JsonIndices);

                    JsonLODDatas.Emplace(MakeShareable(new FJsonValueObject(JsonLODSingle)));

                    LODIndex++;
                }
                JsonRootObject->SetArrayField("LODs", JsonLODDatas);

                FString JsonContent;
                TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonContent, 0);
                if (FJsonSerializer::Serialize(JsonRootObject, JsonWriter))
                {
                    if (FFileHelper::SaveStringToFile(JsonContent, *FullFilePathName))
                    {
                        UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportStaticMesh: success."));

                        return true;
                    }
                }
            }
        }
        else if (FullFilePathName.EndsWith(STATIC_MESH_BINARY_FILE_POSTFIX))
        {
            // Save to binary file
            IFileManager& FileManager = IFileManager::Get();
            FArchive* FileWriter = FileManager.CreateFileWriter(*FullFilePathName);
            if (nullptr == FileWriter)
            {
                UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportStaticMesh: CreateFileWriter failed."));

                return false;
            }

            for (const FStaticMeshLODResources& CurLOD : StaticMesh->RenderData->LODResources)
            {
                // Vertex data
                const FPositionVertexBuffer& PositionVertexBuffer = CurLOD.VertexBuffers.PositionVertexBuffer;
                const FStaticMeshVertexBuffer& StaticMeshVertexBuffer = CurLOD.VertexBuffers.StaticMeshVertexBuffer;
                int32 NumVertices = PositionVertexBuffer.GetNumVertices();

                *FileWriter << NumVertices;

                for (uint32 iVertex = 0; iVertex < PositionVertexBuffer.GetNumVertices(); iVertex++)
                {
                    FVector Position = PositionVertexBuffer.VertexPosition(iVertex);
                    FVector4 TangentZ = StaticMeshVertexBuffer.VertexTangentZ(iVertex);
                    FVector Normal = FVector(TangentZ.X, TangentZ.Y, TangentZ.Z) * TangentZ.W;
                    FVector2D UV = StaticMeshVertexBuffer.GetVertexUV(iVertex, 0);

                    *FileWriter << Position;
                    *FileWriter << Normal;
                    *FileWriter << UV;
                }

                // Index data
                FIndexArrayView Indices = CurLOD.IndexBuffer.GetArrayView();
                int32 NumIndices = Indices.Num();

                *FileWriter << NumIndices;

                for (int32 iIndex = 0; iIndex < Indices.Num(); iIndex++)
                {
                    uint16 Index = Indices[iIndex];
                    *FileWriter << Index;
                }

                //now save only lod 0
                break;
            }

            FileWriter->Close();
            delete FileWriter;
            FileWriter = nullptr;
        }
    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportStaticMesh: failed."));

    return false;
}

bool UObjectExporterBPLibrary::ExportSkeletalMesh(const USkeletalMesh* SkeletalMesh, const FString& FullFilePathName)
{
    FText OutError;
    if (!FFileHelper::IsFilenameValidForSaving(FullFilePathName, OutError))
    {
        UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportSkeletonalMesh: FullFilePathName is not valid. %s"), *OutError.ToString());

        return false;
    }

    if (SkeletalMesh != nullptr)
    {
        if (FullFilePathName.EndsWith(JSON_FILE_POSTFIX))
        {
            const int32 FileVersion = 1;
            TSharedRef<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);
            JsonRootObject->SetNumberField("FileVersion", FileVersion);
        }
        else if (FullFilePathName.EndsWith(SKELETAL_MESH_BINARY_FILE_POSTFIX))
        {
            // Save to binary file
            IFileManager& FileManager = IFileManager::Get();
            FArchive* FileWriter = FileManager.CreateFileWriter(*FullFilePathName);
            if (nullptr == FileWriter)
            {
                UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportSkeletonalMesh: CreateFileWriter failed."));

                return false;
            }

            for (const FSkeletalMeshLODRenderData& CurLOD : SkeletalMesh->GetResourceForRendering()->LODRenderData)
            {
                // Vertex data
                const FPositionVertexBuffer& PositionVertexBuffer = CurLOD.StaticVertexBuffers.PositionVertexBuffer;
                const FStaticMeshVertexBuffer& StaticMeshVertexBuffer = CurLOD.StaticVertexBuffers.StaticMeshVertexBuffer;
                int32 NumVertices = PositionVertexBuffer.GetNumVertices();

                *FileWriter << NumVertices;

                for (uint32 iVertex = 0; iVertex < PositionVertexBuffer.GetNumVertices(); iVertex++)
                {
                    FVector Position = PositionVertexBuffer.VertexPosition(iVertex);
                    FVector4 TangentZ = StaticMeshVertexBuffer.VertexTangentZ(iVertex);
                    FVector Normal = FVector(TangentZ.X, TangentZ.Y, TangentZ.Z);
                    FVector2D UV = StaticMeshVertexBuffer.GetVertexUV(iVertex, 0);

                    *FileWriter << Position;
                    *FileWriter << Normal;
                    *FileWriter << UV;
                }

                // Index data
                TArray<uint32> Indices;
                CurLOD.MultiSizeIndexContainer.GetIndexBuffer(Indices);
                int32 NumIndices = Indices.Num();

                *FileWriter << NumIndices;

                for (int32 iIndex = 0; iIndex < Indices.Num(); iIndex++)
                {
                    uint16 Index = Indices[iIndex];
                    *FileWriter << Index;
                }

                //now save only lod 0
                break;
            }

            FileWriter->Close();
            delete FileWriter;
            FileWriter = nullptr;

        }
    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportStaticMesh: failed."));

    return false;

}

bool UObjectExporterBPLibrary::ExportSkeleton(const USkeleton* Skeleton, const FString& FullFilePathName)
{
    FText OutError;
    if (!FFileHelper::IsFilenameValidForSaving(FullFilePathName, OutError))
    {
        UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportSkeleton: FullFilePathName is not valid. %s"), *OutError.ToString());

        return false;
    }

    if (Skeleton != nullptr)
    {
        if (FullFilePathName.EndsWith(JSON_FILE_POSTFIX))
        {
            const int32 FileVersion = 1;
            TSharedRef<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);
            JsonRootObject->SetNumberField("FileVersion", FileVersion);
        }
        else if (FullFilePathName.EndsWith(SKELETON_BINARY_FILE_POSTFIX))
        {
            // Save to binary file
            IFileManager& FileManager = IFileManager::Get();
            FArchive* FileWriter = FileManager.CreateFileWriter(*FullFilePathName);
            if (nullptr == FileWriter)
            {
                UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportSkeleton: CreateFileWriter failed."));

                return false;
            }

            const TArray<FMeshBoneInfo>& BoneInfos = Skeleton->GetReferenceSkeleton().GetRawRefBoneInfo();
            const TArray<FTransform>& BonePose = Skeleton->GetReferenceSkeleton().GetRawRefBonePose();

            int32 NumBoneInfos = BoneInfos.Num();
            int32 NumPosBones = BonePose.Num();

            *FileWriter << NumBoneInfos;
            for (FMeshBoneInfo Boneinfo : BoneInfos)
            {
                *FileWriter << Boneinfo.Name;
                *FileWriter << Boneinfo.ParentIndex;
            }

            *FileWriter << NumPosBones;
            for (FTransform BoneTransform : BonePose)
            {
                *FileWriter << BoneTransform;
            }

            FileWriter->Close();
            delete FileWriter;
            FileWriter = nullptr;

        }
    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportStaticMesh: failed."));

    return false;

}


bool UObjectExporterBPLibrary::ExportAnimSequence(const UAnimSequence* AnimSequence, const FString& FullFilePathName)
{
    FText OutError;
    if (!FFileHelper::IsFilenameValidForSaving(FullFilePathName, OutError))
    {
        UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportAnimSequence: FullFilePathName is not valid. %s"), *OutError.ToString());

        return false;
    }

    if (AnimSequence != nullptr)
    {
        if (FullFilePathName.EndsWith(JSON_FILE_POSTFIX))
        {
            const int32 FileVersion = 1;
            TSharedRef<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);
            JsonRootObject->SetNumberField("FileVersion", FileVersion);
        }
        else if (FullFilePathName.EndsWith(ANIMSEQUENCE_BINARY_FILE_POSTFIX))
        {
            // Save to binary file
            IFileManager& FileManager = IFileManager::Get();
            FArchive* FileWriter = FileManager.CreateFileWriter(*FullFilePathName);
            if (nullptr == FileWriter)
            {
                UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportAnimSequence: CreateFileWriter failed."));

                return false;
            }
     
            const TArray<FRawAnimSequenceTrack>& AnimationData = AnimSequence->GetRawAnimationData();

            int32 NumberOfFrames = AnimationData.Num();
            *FileWriter << NumberOfFrames;

            for (FRawAnimSequenceTrack SequenceTrack : AnimationData)
            {
                *FileWriter << SequenceTrack.ScaleKeys;
                *FileWriter << SequenceTrack.RotKeys;
                *FileWriter << SequenceTrack.PosKeys;
            }

            FileWriter->Close();
            delete FileWriter;
            FileWriter = nullptr;

        }
    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportStaticMesh: failed."));

    return false;

}


bool UObjectExporterBPLibrary::ExportCamera(const UCameraComponent* Camera, const FString& FullFilePathName)
{
    FText OutError;
    if (!FFileHelper::IsFilenameValidForSaving(FullFilePathName, OutError))
    {
        UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportCamera: FullFilePathName is not valid. %s"), *OutError.ToString());

        return false;
    }

    if (Camera != nullptr)
    {
        const int32 FileVersion = 1;
        TSharedRef<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);
        JsonRootObject->SetNumberField("FileVersion", FileVersion);

        TSharedRef<FJsonObject> JsonCamera = MakeShareable(new FJsonObject);

        const FVector Position = Camera->GetComponentLocation();
        TSharedRef<FJsonObject> JsonPosition = MakeShareable(new FJsonObject);
        JsonPosition->SetNumberField("x", Position.X);
        JsonPosition->SetNumberField("y", Position.Y);
        JsonPosition->SetNumberField("z", Position.Z);
        JsonCamera->SetObjectField("Location", JsonPosition);

        const FRotator Rotation = Camera->GetComponentRotation();
        TSharedRef<FJsonObject> JsonRotation = MakeShareable(new FJsonObject);
        JsonRotation->SetNumberField("roll", Rotation.Roll);
        JsonRotation->SetNumberField("yaw", Rotation.Yaw);
        JsonRotation->SetNumberField("pitch", Rotation.Pitch);
        JsonCamera->SetObjectField("Rotation", JsonRotation);

        JsonCamera->SetNumberField("FOV", Camera->FieldOfView);
        JsonCamera->SetNumberField("AspectRatio", Camera->AspectRatio);

        JsonRootObject->SetObjectField("Camera", JsonCamera);

        FString JsonContent;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonContent, 0);
        if (FJsonSerializer::Serialize(JsonRootObject, JsonWriter))
        {
            if (FFileHelper::SaveStringToFile(JsonContent, *FullFilePathName))
            {
                UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportCamera: success."));

                return true;
            }
        }
    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportCamera: failed."));

    return false;
}

bool UObjectExporterBPLibrary::ExportMap(UObject* WorldContextObject, const FString& FullFilePathName)
{
    if (!IsValid(WorldContextObject) || !IsValid(WorldContextObject->GetWorld()))
    {
        return false;
    }

    if (FullFilePathName.EndsWith(MAP_BINARY_FILE_POSTFIX))
    {
        // Save to binary file
        IFileManager& FileManager = IFileManager::Get();
        FArchive* FileWriter = FileManager.CreateFileWriter(*FullFilePathName);
        if (nullptr == FileWriter)
        {
            UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportMap: CreateFileWriter failed."));

            return false;
        }

        UWorld* World = WorldContextObject->GetWorld();

        TArray<AActor*> AllCameraActors;
        UGameplayStatics::GetAllActorsOfClass(World, ACameraActor::StaticClass(), AllCameraActors);
        int32 CameraCount = AllCameraActors.Num();

        *FileWriter << CameraCount;

        for (AActor* Actor : AllCameraActors)
        {
            UCameraComponent* Component = Cast<UCameraComponent>(Actor->GetComponentByClass(UCameraComponent::StaticClass()));
            check(Component != nullptr);
            auto Transform = Component->GetComponentToWorld();
            auto Location = Transform.GetLocation();
            auto Rotation = Transform.GetRotation();
            auto Rotator = Rotation.Rotator();
            auto Direction = Rotation.Vector();
            auto Target = Location + Direction * 100.0f;
            auto FOV = Component->FieldOfView;
            auto AspectRatio = Component->AspectRatio;

            *FileWriter << Location;
            *FileWriter << Target;
            *FileWriter << FOV;
            *FileWriter << AspectRatio;
        }

        TArray<AActor*> AllDirectionalLightActors;
        UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), AllDirectionalLightActors);
        int32 DirectionalLightCount = AllDirectionalLightActors.Num();

        *FileWriter << DirectionalLightCount;

        for (AActor* Actor : AllDirectionalLightActors)
        {
            UDirectionalLightComponent* Component = Cast<UDirectionalLightComponent>(Actor->GetComponentByClass(UDirectionalLightComponent::StaticClass()));
            check(Component != nullptr);
            auto Transform = Component->GetComponentToWorld();
            auto Location = Transform.GetLocation();
            auto Rotation = Transform.GetRotation();
            auto Rotator = Rotation.Rotator();
            auto Direction = Rotation.Vector();
            auto Color = FLinearColor::FromSRGBColor(Component->LightColor);
            auto Intensity = Component->Intensity;

            *FileWriter << Color;
            *FileWriter << Direction;
            *FileWriter << Intensity;
        }

        TArray<AActor*> AllStaticMeshActors;
        UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), AllStaticMeshActors);
        int32 StaticMeshActorCount = AllStaticMeshActors.Num();

        *FileWriter << StaticMeshActorCount;

        for (AActor* Actor : AllStaticMeshActors)
        {
            UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Actor->GetComponentByClass(UStaticMeshComponent::StaticClass()));
            check(Component != nullptr);
            auto Transform = Component->GetComponentToWorld();
            auto Location = Transform.GetLocation();
            auto Rotation = Transform.GetRotation();
            auto Rotator = Rotation.Rotator();
            auto Direction = Rotation.Vector();
            auto ResourceFullName = Component->GetStaticMesh()->GetPathName();

            FString ResourcePath, ResourceName;
            ResourceFullName.Split(FString("."), &ResourcePath, &ResourceName);

            *FileWriter << Rotation;
            *FileWriter << Location;
            *FileWriter << ResourceName;

            TArray<UTexture*> MaterialTextures;
            Component->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num);
            FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
            for (UTexture* Texture : MaterialTextures)
            {
                FString SavePath = FPaths::ProjectSavedDir() + "Bin/Textures";
                TArray<UObject*> ObjectsToExport;
                ObjectsToExport.Add(Texture);
                AssetToolsModule.Get().ExportAssets(ObjectsToExport, *SavePath);
            }

            FString SaveStaticMeshPath = FPaths::ProjectSavedDir() + "Bin/StaticMesh/" + ResourceName + STATIC_MESH_BINARY_FILE_POSTFIX;
            ExportStaticMesh(Component->GetStaticMesh(), SaveStaticMeshPath);
        }

        TArray<AActor*> AllSkeletalMeshActors;
        UGameplayStatics::GetAllActorsOfClass(World, ASkeletalMeshActor::StaticClass(), AllSkeletalMeshActors);
        int32 SkeletalMeshActorCount = AllSkeletalMeshActors.Num();

        *FileWriter << SkeletalMeshActorCount;

        for (AActor* Actor : AllSkeletalMeshActors)
        {
            USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(Actor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
            check(Component != nullptr);
            auto Transform = Component->GetComponentToWorld();
            auto Location = Transform.GetLocation();
            auto Rotation = Transform.GetRotation();
            auto Rotator = Rotation.Rotator();
            auto Direction = Rotation.Vector();
            auto ResourceFullName = Component->SkeletalMesh->GetPathName();

            FString ResourcePath, ResourceName;
            ResourceFullName.Split(FString("."), &ResourcePath, &ResourceName);

            *FileWriter << Rotation;
            *FileWriter << Location;
            *FileWriter << ResourceName;

            TArray<UTexture*> MaterialTextures;
            Component->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num);
            FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
            for (UTexture* Texture : MaterialTextures)
            {
                FString SavePath = FPaths::ProjectSavedDir() + "Bin/Textures";
                TArray<UObject*> ObjectsToExport;
                ObjectsToExport.Add(Texture);
                AssetToolsModule.Get().ExportAssets(ObjectsToExport, *SavePath);
            }

            FString SaveSkeletalMeshPath = FPaths::ProjectSavedDir() + "Bin/SkeletalMesh/" + ResourceName + SKELETAL_MESH_BINARY_FILE_POSTFIX;
            ExportSkeletalMesh(Component->SkeletalMesh, SaveSkeletalMeshPath);

            FString SaveSkeletonPath = FPaths::ProjectSavedDir() + "Bin/SkeletalMesh/Skeleton/" + ResourceName + SKELETON_BINARY_FILE_POSTFIX;
            ExportSkeleton(Component->SkeletalMesh->Skeleton, SaveSkeletonPath);
 
            FString SaveAnimSequencePath = FPaths::ProjectSavedDir() + "Bin/SkeletalMesh/AnimSequence/" + ResourceName + ANIMSEQUENCE_BINARY_FILE_POSTFIX;
            ExportAnimSequence(Cast<UAnimSequence>(Component->AnimationData.AnimToPlay), SaveAnimSequencePath);
        }


        FileWriter->Close();
        delete FileWriter;
        FileWriter = nullptr;

        UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportMap: success."));

        return true;

    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportMap: failed."));

    return false;
}
