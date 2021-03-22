// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectExporterBPLibrary.h"
#include "ObjectExporter.h"
#include "Camera/CameraComponent.h"

#define JSON_FILE_POSTFIX ".json"
#define BINARY_FILE_POSTFIX ".stm"

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
        else if (FullFilePathName.EndsWith(BINARY_FILE_POSTFIX))
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
                    FVector Normal = StaticMeshVertexBuffer.VertexTangentZ(iVertex);
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

