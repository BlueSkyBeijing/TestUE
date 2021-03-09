// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectExporterBPLibrary.h"
#include "ObjectExporter.h"
#include "Camera/CameraComponent.h"


DECLARE_LOG_CATEGORY_CLASS(ObjectExporterBPLibraryLog, Log, All);

UObjectExporterBPLibrary::UObjectExporterBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

bool UObjectExporterBPLibrary::ExportStaticMesh(const UStaticMesh* StaticMesh, const FString& FilePath)
{
    if (StaticMesh != nullptr)
    {
        if (!StaticMesh->bAllowCPUAccess)
        {
            UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportStaticMesh: StaticMesh must AllowCPUAccess."));

            return false;
        }

        const int32 FileVersion = 1;
        TSharedRef<FJsonObject> JsonRootObject = MakeShareable(new FJsonObject);
        JsonRootObject->SetNumberField("FileVersion", FileVersion);
        JsonRootObject->SetStringField("MeshName", *StaticMesh->GetName());

        if (StaticMesh->RenderData != nullptr)
        {
            // Vertex format
            TArray<TSharedPtr<FJsonValue>> JsonVertexFormat;
            JsonRootObject->SetArrayField("VertexFormat", JsonVertexFormat);

            int32 LODIndex = 0;
            TArray< TSharedPtr<FJsonValue> > JsonLODDatas;
            for (const FStaticMeshLODResources& CurLOD : StaticMesh->RenderData->LODResources)
            {
                TSharedRef<FJsonObject> JsonLODSingle = MakeShareable(new FJsonObject);
                JsonLODSingle->SetNumberField("LOD", LODIndex);

                // Vertex data
                TArray<TSharedPtr<FJsonValue>> JsonVertices;
                const FPositionVertexBuffer& VertexBuffer = CurLOD.VertexBuffers.PositionVertexBuffer;
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
                FIndexArrayView Indices = CurLOD.IndexBuffer.GetArrayView();
                for (int32 iIndex = 0; iIndex < Indices.Num(); iIndex++)
                {
                    TSharedRef<FJsonObject> JsonIndex = MakeShareable(new FJsonObject);
                    JsonIndex->SetNumberField("index", Indices[iIndex]);

                    JsonVertices.Emplace(MakeShareable(new FJsonValueObject(JsonIndex)));
                }
                JsonLODSingle->SetArrayField("Indices", JsonVertices);

                JsonLODDatas.Emplace(MakeShareable(new FJsonValueObject(JsonLODSingle)));

                LODIndex++;
            }
            JsonRootObject->SetArrayField("LODs", JsonLODDatas);

            FString JsonContent;
            TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonContent, 0);
            if (FJsonSerializer::Serialize(JsonRootObject, JsonWriter))
            {
                if (FFileHelper::SaveStringToFile(JsonContent, *FilePath))
                {
                    UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportStaticMesh: success."));
                    
                    return true;
                }
            }
        }
    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportStaticMesh: failed."));

    return false;
}

bool UObjectExporterBPLibrary::ExportCamera(const UCameraComponent* Camera, const FString& FilePath)
{
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
            if (FFileHelper::SaveStringToFile(JsonContent, *FilePath))
            {
                UE_LOG(ObjectExporterBPLibraryLog, Log, TEXT("ExportCamera: success."));

                return true;
            }
        }
    }

    UE_LOG(ObjectExporterBPLibraryLog, Warning, TEXT("ExportCamera: failed."));

    return false;
}

