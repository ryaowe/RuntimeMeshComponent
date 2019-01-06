// Copyright 2016-2018 Chris Conway (Koderz). All Rights Reserved.

#include "RuntimeMeshSection.h"
#include "RuntimeMeshComponentPlugin.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "RuntimeMeshUpdateCommands.h"

template<typename Type>
struct FRuntimeMeshStreamAccessor
{
	const TArray<uint8>* Data;
	int32 Offset;
	int32 Stride;
public:
	FRuntimeMeshStreamAccessor(const TArray<uint8>* InData, int32 InOffset, int32 InStride)
		: Data(InData), Offset(InOffset), Stride(InStride)
	{
	}
	virtual ~FRuntimeMeshStreamAccessor() { }

	int32 Num() const { return Data->Num() / Stride; }

	Type& Get(int32 Index)
	{
		int32 StartPosition = (Index * Stride + Offset);
		return *((Type*)(&(*Data)[StartPosition]));
	}
};

// Helper for accessing position element within a vertex stream
struct FRuntimeMeshVertexStreamPositionAccessor : public FRuntimeMeshStreamAccessor<FVector>
{
public:
	FRuntimeMeshVertexStreamPositionAccessor(TArray<uint8>* InData, const FRuntimeMeshVertexStreamStructure& StreamStructure)
		: FRuntimeMeshStreamAccessor<FVector>(InData, StreamStructure.Position.Offset, StreamStructure.Position.Stride)
	{
		check(StreamStructure.Position.IsValid());
	}
};

struct FRuntimeMeshVertexStreamUVAccessor
{
	virtual ~FRuntimeMeshVertexStreamUVAccessor() { }

	virtual FVector2D GetUV(int32 Index) = 0;
	virtual int32 Num() = 0;
};

// Helper for accessing position element within a vertex stream
struct FRuntimeMeshVertexStreamUVFullPrecisionAccessor : public FRuntimeMeshStreamAccessor<FVector2D>, public FRuntimeMeshVertexStreamUVAccessor
{
public:
	FRuntimeMeshVertexStreamUVFullPrecisionAccessor(TArray<uint8>* InData, const FRuntimeMeshVertexStreamStructureElement& Element)
		: FRuntimeMeshStreamAccessor<FVector2D>(InData, Element.Offset, Element.Stride)
	{
		check(Element.IsValid());
	}

	virtual FVector2D GetUV(int32 Index) override
	{
		return Get(Index);
	}

	virtual int32 Num() override
	{
		return FRuntimeMeshStreamAccessor<FVector2D>::Num();
	}
};
struct FRuntimeMeshVertexStreamUVHalfPrecisionAccessor : public FRuntimeMeshStreamAccessor<FVector2DHalf>, public FRuntimeMeshVertexStreamUVAccessor
{
public:
	FRuntimeMeshVertexStreamUVHalfPrecisionAccessor(TArray<uint8>* InData, const FRuntimeMeshVertexStreamStructureElement& Element)
		: FRuntimeMeshStreamAccessor<FVector2DHalf>(InData, Element.Offset, Element.Stride)
	{
		check(Element.IsValid());
	}

	virtual FVector2D GetUV(int32 Index) override
	{
		return Get(Index);
	}

	virtual int32 Num() override
	{
		return FRuntimeMeshStreamAccessor<FVector2DHalf>::Num();
	}
};


void FRuntimeMeshSectionVertexBuffer::FillUpdateParams(FRuntimeMeshSectionVertexBufferParams& Params)
{
	Params.Data = Data;
	Params.NumVertices = GetNumVertices();
}

void FRuntimeMeshSectionIndexBuffer::FillUpdateParams(FRuntimeMeshSectionIndexBufferParams& Params)
{
	Params.b32BitIndices = b32BitIndices;
	Params.Data = Data;
	Params.NumIndices = GetNumIndices();
}

void FRuntimeMeshSectionTangentsVertexBuffer::FillUpdateParams(FRuntimeMeshSectionTangentVertexBufferParams& Params)
{
	Params.bUsingHighPrecision = bUseHighPrecision;
	FRuntimeMeshSectionVertexBuffer::FillUpdateParams(Params);
}

void FRuntimeMeshSectionUVsVertexBuffer::FillUpdateParams(FRuntimeMeshSectionUVVertexBufferParams& Params)
{
	Params.bUsingHighPrecision = bUseHighPrecision;
	Params.NumUVs = UVCount;
	FRuntimeMeshSectionVertexBuffer::FillUpdateParams(Params);
}



FRuntimeMeshSection::FRuntimeMeshSection(bool bInUseHighPrecisionTangents, bool bInUseHighPrecisionUVs, int32 InNumUVs, bool b32BitIndices, EUpdateFrequency InUpdateFrequency)
	: UpdateFrequency(InUpdateFrequency)
	, LocalBoundingBox(EForceInit::ForceInitToZero)
	, bCollisionEnabled(false)
	, bIsVisible(true)
	, bCastsShadow(true)
{
	LODs.Emplace(bInUseHighPrecisionTangents, bInUseHighPrecisionUVs, InNumUVs, b32BitIndices);	
}

FRuntimeMeshSection::FRuntimeMeshSection(FArchive& Ar)
	: UpdateFrequency(EUpdateFrequency::Average)
	, LocalBoundingBox(EForceInit::ForceInitToZero)
	, bCollisionEnabled(false)
	, bIsVisible(true)
	, bCastsShadow(true)
{
	Ar << *this;
}

FRuntimeMeshSectionCreationParamsPtr FRuntimeMeshSection::GetSectionCreationParams()
{
	FRuntimeMeshSectionCreationParamsPtr CreationParams = MakeShared<FRuntimeMeshSectionCreationParams, ESPMode::NotThreadSafe>();

	CreationParams->UpdateFrequency = UpdateFrequency;

	CreationParams->LODs.SetNum(LODs.Num());
	for (int32 Index = 0; Index < LODs.Num(); Index++)
	{
		LODs[Index].PositionBuffer.FillUpdateParams(CreationParams->LODs[Index].PositionVertexBuffer);
		LODs[Index].TangentsBuffer.FillUpdateParams(CreationParams->LODs[Index].TangentsVertexBuffer);
		LODs[Index].UVsBuffer.FillUpdateParams(CreationParams->LODs[Index].UVsVertexBuffer);
		LODs[Index].ColorBuffer.FillUpdateParams(CreationParams->LODs[Index].ColorVertexBuffer);

		LODs[Index].IndexBuffer.FillUpdateParams(CreationParams->LODs[Index].IndexBuffer);
		LODs[Index].AdjacencyIndexBuffer.FillUpdateParams(CreationParams->LODs[Index].AdjacencyIndexBuffer);
	}

	CreationParams->bIsVisible = bIsVisible;
	CreationParams->bCastsShadow = bCastsShadow;

	return CreationParams;
}

FRuntimeMeshSectionUpdateParamsPtr FRuntimeMeshSection::GetSectionUpdateData(int32 LODIndex, ERuntimeMeshBuffersToUpdate BuffersToUpdate)
{
	FRuntimeMeshSectionUpdateParamsPtr UpdateParams = MakeShared<FRuntimeMeshSectionUpdateParams, ESPMode::NotThreadSafe>();

	UpdateParams->LODIndex = LODIndex;
	UpdateParams->BuffersToUpdate = BuffersToUpdate;

	if (!!(BuffersToUpdate & ERuntimeMeshBuffersToUpdate::PositionBuffer))
	{
		LODs[LODIndex].PositionBuffer.FillUpdateParams(UpdateParams->PositionVertexBuffer);
	}

	if (!!(BuffersToUpdate & ERuntimeMeshBuffersToUpdate::TangentBuffer))
	{
		LODs[LODIndex].TangentsBuffer.FillUpdateParams(UpdateParams->TangentsVertexBuffer);
	}

	if (!!(BuffersToUpdate & ERuntimeMeshBuffersToUpdate::UVBuffer))
	{
		LODs[LODIndex].UVsBuffer.FillUpdateParams(UpdateParams->UVsVertexBuffer);
	}

	if (!!(BuffersToUpdate & ERuntimeMeshBuffersToUpdate::ColorBuffer))
	{
		LODs[LODIndex].ColorBuffer.FillUpdateParams(UpdateParams->ColorVertexBuffer);
	}

	if (!!(BuffersToUpdate & ERuntimeMeshBuffersToUpdate::IndexBuffer))
	{
		LODs[LODIndex].IndexBuffer.FillUpdateParams(UpdateParams->IndexBuffer);
	}

	if (!!(BuffersToUpdate & ERuntimeMeshBuffersToUpdate::AdjacencyIndexBuffer))
	{
		LODs[LODIndex].AdjacencyIndexBuffer.FillUpdateParams(UpdateParams->AdjacencyIndexBuffer);
	}

	return UpdateParams;
}

TSharedPtr<struct FRuntimeMeshSectionPropertyUpdateParams, ESPMode::NotThreadSafe> FRuntimeMeshSection::GetSectionPropertyUpdateData()
{
	FRuntimeMeshSectionPropertyUpdateParamsPtr UpdateParams = MakeShared<FRuntimeMeshSectionPropertyUpdateParams, ESPMode::NotThreadSafe>();

	UpdateParams->bCastsShadow = bCastsShadow;
	UpdateParams->bIsVisible = bIsVisible;

	return UpdateParams;
}

void FRuntimeMeshSection::UpdateBoundingBox()
{
	FBox NewBoundingBox(reinterpret_cast<FVector*>(LODs[0].PositionBuffer.GetData().GetData()), LODs[0].PositionBuffer.GetNumVertices());
	
	LocalBoundingBox = NewBoundingBox;
}

int32 FRuntimeMeshSection::GetCollisionData(int32 LODIndex, TArray<FVector>& OutPositions, TArray<FTriIndices>& OutIndices, TArray<FVector2D>& OutUVs)
{ 
 	int32 StartVertexPosition = OutPositions.Num();

	FRuntimeMeshSectionLODData& LODData = LODs[FMath::Clamp(LODIndex, 0, LODs.Num()-1)];

	OutPositions.Append(reinterpret_cast<FVector*>(LODData.PositionBuffer.GetData().GetData()), LODData.PositionBuffer.GetNumVertices());
  
 	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
 
 	//if (bCopyUVs)
 	//{
 	//	TUniquePtr<FRuntimeMeshVertexStreamUVAccessor> StreamAccessor = nullptr;
 
 	//	const auto SetupStreamAccessor = [](TArray<uint8>* Data, const FRuntimeMeshVertexStreamStructureElement& Element) -> TUniquePtr<FRuntimeMeshVertexStreamUVAccessor>
 	//	{
 	//		if (Element.Type == VET_Float2 || Element.Type == VET_Float4)
 	//		{
 	//			return MakeUnique<FRuntimeMeshVertexStreamUVFullPrecisionAccessor>(Data, Element);
 	//		}
 	//		else
 	//		{
 	//			check(Element.Type == VET_Half2 || Element.Type == VET_Half4);
 	//			return MakeUnique<FRuntimeMeshVertexStreamUVHalfPrecisionAccessor>(Data, Element);
 	//		}
 	//	};
 
 	//	if (VertexBuffer0.GetStructure().HasUVs())
 	//	{
 	//		StreamAccessor = SetupStreamAccessor(&VertexBuffer0.GetData(), VertexBuffer0.GetStructure().UVs[0]);
 	//	}
 	//	else if (VertexBuffer1.GetStructure().HasUVs())
 	//	{
 	//		StreamAccessor = SetupStreamAccessor(&VertexBuffer1.GetData(), VertexBuffer1.GetStructure().UVs[0]);
 	//	}
 	//	else if (VertexBuffer2.GetStructure().HasUVs())
 	//	{
 	//		StreamAccessor = SetupStreamAccessor(&VertexBuffer2.GetData(), VertexBuffer2.GetStructure().UVs[0]);
 	//	}
 	//	else
 	//	{
 	//		// Add blank entries since we can't get UV's for this section
 	//		OutUVs.AddZeroed(PositionAccessor.Num());
 	//	}
 
 	//	if (StreamAccessor.IsValid())
 	//	{
 	//		OutUVs.Reserve(OutUVs.Num() + StreamAccessor->Num());
 	//		for (int32 Index = 0; Index < StreamAccessor->Num(); Index++)
 	//		{
 	//			OutUVs.Add(StreamAccessor->GetUV(Index));
 	//		}
 	//	}
 	//}
 
 	TArray<uint8>& IndexData = LODData.IndexBuffer.GetData();
 
 	if (LODData.IndexBuffer.Is32BitIndices())
 	{
 		int32 NumIndices = LODData.IndexBuffer.GetNumIndices();
 		for (int32 Index = 0; Index < NumIndices; Index += 3)
 		{
 			// Add the triangle
 			FTriIndices& Triangle = *new (OutIndices) FTriIndices;
 			Triangle.v0 = (*((int32*)&IndexData[(Index + 0) * 4])) + StartVertexPosition;
 			Triangle.v1 = (*((int32*)&IndexData[(Index + 1) * 4])) + StartVertexPosition;
 			Triangle.v2 = (*((int32*)&IndexData[(Index + 2) * 4])) + StartVertexPosition;
 		}
 	}
 	else
 	{
 		int32 NumIndices = LODData.IndexBuffer.GetNumIndices();
 		for (int32 Index = 0; Index < NumIndices; Index += 3)
 		{
 			// Add the triangle
 			FTriIndices& Triangle = *new (OutIndices) FTriIndices;
 			Triangle.v0 = (*((uint16*)&IndexData[(Index + 0) * 2])) + StartVertexPosition;
 			Triangle.v1 = (*((uint16*)&IndexData[(Index + 1) * 2])) + StartVertexPosition;
 			Triangle.v2 = (*((uint16*)&IndexData[(Index + 2) * 2])) + StartVertexPosition;
 		}
 	}


	return LODData.IndexBuffer.GetNumIndices() / 3;
}

