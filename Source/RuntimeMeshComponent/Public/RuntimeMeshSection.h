// Copyright 2016-2018 Chris Conway (Koderz). All Rights Reserved.

#pragma once

#include "Engine.h"
#include "RuntimeMeshCore.h"
#include "RuntimeMeshBuilder.h"

enum class ERuntimeMeshBuffersToUpdate : uint8;
struct FRuntimeMeshSectionVertexBufferParams;
struct FRuntimeMeshSectionTangentVertexBufferParams;
struct FRuntimeMeshSectionUVVertexBufferParams;
struct FRuntimeMeshSectionIndexBufferParams;
class UMaterialInterface;


struct FRuntimeMeshSectionVertexBuffer
{
private:
	const int32 Stride;
	TArray<uint8> Data;
public:
	FRuntimeMeshSectionVertexBuffer(int32 InStride) : Stride(InStride)
	{

	}
	virtual ~FRuntimeMeshSectionVertexBuffer() { }

	void SetData(TArray<uint8>& InVertices, bool bUseMove)
	{
		if (bUseMove)
		{
			Data = MoveTemp(InVertices);
		}
		else
		{
			Data = InVertices;
		}
	}

	template<typename VertexType>
	void SetData(const TArray<VertexType>& InVertices)
	{
		if (InVertices.Num() == 0)
		{
			Data.Empty();
			return;
		}
		check(InVertices.GetTypeSize() == GetStride());

		Data.SetNum(InVertices.GetTypeSize() * InVertices.Num());
		FMemory::Memcpy(Data.GetData(), InVertices.GetData(), Data.Num());
	}

	int32 GetStride() const
	{
		return Stride;
	}

	int32 GetNumVertices() const
	{
		return Stride > 0 ? Data.Num() / Stride : 0;
	}

	TArray<uint8>& GetData() { return Data; }

	void FillUpdateParams(FRuntimeMeshSectionVertexBufferParams& Params);

	friend FArchive& operator <<(FArchive& Ar, FRuntimeMeshSectionVertexBuffer& Buffer)
	{
		Buffer.Serialize(Ar);
		return Ar;
	}

protected:
	virtual void Serialize(FArchive& Ar)
	{
		if (Ar.CustomVer(FRuntimeMeshVersion::GUID) < FRuntimeMeshVersion::RuntimeMeshComponentUE4_19)
		{
			FRuntimeMeshVertexStreamStructure VertexStructure;
			Ar << const_cast<FRuntimeMeshVertexStreamStructure&>(VertexStructure);
		}
		Ar << const_cast<int32&>(Stride);
		Ar << Data;
	}
};

struct FRuntimeMeshSectionPositionVertexBuffer : public FRuntimeMeshSectionVertexBuffer
{
	FRuntimeMeshSectionPositionVertexBuffer()
		: FRuntimeMeshSectionVertexBuffer(sizeof(FVector))
	{

	}
};

struct FRuntimeMeshSectionTangentsVertexBuffer : public FRuntimeMeshSectionVertexBuffer
{
private:
	bool bUseHighPrecision;

public:
	FRuntimeMeshSectionTangentsVertexBuffer() : FRuntimeMeshSectionVertexBuffer(sizeof(FPackedNormal) * 2), bUseHighPrecision(false) { }
	FRuntimeMeshSectionTangentsVertexBuffer(bool bInUseHighPrecision)
		: FRuntimeMeshSectionVertexBuffer(bInUseHighPrecision ? (sizeof(FPackedRGBA16N) * 2) : (sizeof(FPackedNormal) * 2))
		, bUseHighPrecision(bInUseHighPrecision)
	{

	}

	bool IsUsingHighPrecision() const { return bUseHighPrecision; }

	void FillUpdateParams(FRuntimeMeshSectionTangentVertexBufferParams& Params);

	virtual void Serialize(FArchive& Ar) override
	{
		if (Ar.CustomVer(FRuntimeMeshVersion::GUID) >= FRuntimeMeshVersion::RuntimeMeshComponentUE4_19)
		{
			Ar << bUseHighPrecision;
		}
		FRuntimeMeshSectionVertexBuffer::Serialize(Ar);
	}
};

struct FRuntimeMeshSectionUVsVertexBuffer : public FRuntimeMeshSectionVertexBuffer
{
private:
	bool bUseHighPrecision;
	int32 UVCount;

public:

	FRuntimeMeshSectionUVsVertexBuffer() : FRuntimeMeshSectionVertexBuffer(sizeof(FVector2DHalf)), bUseHighPrecision(false), UVCount(1) { }
	FRuntimeMeshSectionUVsVertexBuffer(bool bInUseHighPrecision, int32 InUVCount)
		: FRuntimeMeshSectionVertexBuffer((bInUseHighPrecision ? sizeof(FVector2D) : sizeof(FVector2DHalf)) * InUVCount)
		, bUseHighPrecision(bInUseHighPrecision), UVCount(InUVCount)
	{

	}

	bool IsUsingHighPrecision() const { return bUseHighPrecision; }

	int32 NumUVs() const { return UVCount; }

	void FillUpdateParams(FRuntimeMeshSectionUVVertexBufferParams& Params);

	virtual void Serialize(FArchive& Ar) override
	{
		if (Ar.CustomVer(FRuntimeMeshVersion::GUID) >= FRuntimeMeshVersion::RuntimeMeshComponentUE4_19)
		{
			Ar << bUseHighPrecision;
			Ar << UVCount;
		}
		FRuntimeMeshSectionVertexBuffer::Serialize(Ar);
	}
};

struct FRuntimeMeshSectionColorVertexBuffer : public FRuntimeMeshSectionVertexBuffer
{
	FRuntimeMeshSectionColorVertexBuffer()
		: FRuntimeMeshSectionVertexBuffer(sizeof(FColor))
	{

	}
};

struct FRuntimeMeshSectionIndexBuffer
{
private:
	const bool b32BitIndices;
	TArray<uint8> Data;
public:
	FRuntimeMeshSectionIndexBuffer() : b32BitIndices(false) { }
	FRuntimeMeshSectionIndexBuffer(bool bIn32BitIndices)
		: b32BitIndices(bIn32BitIndices)
	{

	}

	void SetData(TArray<uint8>& InIndices, bool bUseMove)
	{
		if (bUseMove)
		{
			Data = MoveTemp(InIndices);
		}
		else
		{
			Data = InIndices;
		}
	}

	template<typename IndexType>
	void SetData(const TArray<IndexType>& InIndices)
	{
		check(InIndices.GetTypeSize() == GetStride());

		Data.SetNum(InIndices.GetTypeSize() * InIndices.Num());
		FMemory::Memcpy(Data.GetData(), InIndices.GetData(), Data.Num());
	}

	int32 GetStride() const
	{
		return b32BitIndices ? 4 : 2;
	}

	bool Is32BitIndices() const
	{
		return b32BitIndices;
	}

	int32 GetNumIndices() const
	{
		return Data.Num() / GetStride();
	}

	TArray<uint8>& GetData() { return Data; }

	void FillUpdateParams(FRuntimeMeshSectionIndexBufferParams& Params);

	friend FArchive& operator <<(FArchive& Ar, FRuntimeMeshSectionIndexBuffer& Buffer)
	{
		Ar << const_cast<bool&>(Buffer.b32BitIndices);
		Ar << Buffer.Data;
		return Ar;
	}
};



class FRuntimeMeshSectionLODData
{	
public:

	/** Vertex buffer containing the positions for this section */
	FRuntimeMeshSectionPositionVertexBuffer PositionBuffer;

	/** Vertex buffer containing the tangents for this section */
	FRuntimeMeshSectionTangentsVertexBuffer TangentsBuffer;

	/** Vertex buffer containing the UVs for this section */
	FRuntimeMeshSectionUVsVertexBuffer UVsBuffer;

	/** Vertex buffer containing the colors for this section */
	FRuntimeMeshSectionColorVertexBuffer ColorBuffer;

	FRuntimeMeshSectionIndexBuffer IndexBuffer;

	FRuntimeMeshSectionIndexBuffer AdjacencyIndexBuffer;

	FRuntimeMeshSectionLODData() { }
	FRuntimeMeshSectionLODData(bool bInUseHighPrecisionTangents, bool bInUseHighPrecisionUVs, int32 InNumUVs, bool b32BitIndices)
		: TangentsBuffer(bInUseHighPrecisionTangents)
		, UVsBuffer(bInUseHighPrecisionUVs, InNumUVs)
		, IndexBuffer(b32BitIndices)
		, AdjacencyIndexBuffer(b32BitIndices)
	{

	}


	bool HasValidMeshData() const 
	{
		if (IndexBuffer.GetNumIndices() <= 0)
			return false;
		if (PositionBuffer.GetNumVertices() <= 0)
			return false;
		if (TangentsBuffer.GetNumVertices() != 0 && TangentsBuffer.GetNumVertices() != PositionBuffer.GetNumVertices())
			return false;
		if (UVsBuffer.GetNumVertices() != 0 && UVsBuffer.GetNumVertices() != PositionBuffer.GetNumVertices())
			return false;
		if (ColorBuffer.GetNumVertices() != 0 && ColorBuffer.GetNumVertices() != PositionBuffer.GetNumVertices())
			return false;
		return true;
	}

	int32 GetNumVertices() const { return PositionBuffer.GetNumVertices(); }
	int32 GetNumIndices() const { return IndexBuffer.GetNumIndices(); }
		
	void UpdatePositionBuffer(TArray<uint8>& InVertices, bool bUseMove)
	{
		PositionBuffer.SetData(InVertices, bUseMove);
	}

	void UpdateTangentsBuffer(TArray<uint8>& InVertices, bool bUseMove)
	{
		TangentsBuffer.SetData(InVertices, bUseMove);
	}

	template<typename VertexType>
	void UpdateTangentsBuffer(const TArray<VertexType>& InVertices)
	{
		TangentsBuffer.SetData(InVertices);
	}

	void UpdateUVsBuffer(TArray<uint8>& InVertices, bool bUseMove)
	{
		UVsBuffer.SetData(InVertices, bUseMove);
	}

	template<typename VertexType>
	void UpdateUVsBuffer(const TArray<VertexType>& InVertices)
	{
		UVsBuffer.SetData(InVertices);
	}

	void UpdateColorBuffer(TArray<uint8>& InVertices, bool bUseMove)
	{
		ColorBuffer.SetData(InVertices, bUseMove);
	}

	template<typename VertexType>
	void UpdateColorBuffer(const TArray<VertexType>& InVertices)
	{
		ColorBuffer.SetData(InVertices);
	}

	void UpdateIndexBuffer(TArray<uint8>& InIndices, bool bUseMove)
	{
		IndexBuffer.SetData(InIndices, bUseMove);
	}

	template<typename IndexType>
	void UpdateIndexBuffer(const TArray<IndexType>& InIndices)
	{
		IndexBuffer.SetData(InIndices);
	}

	template<typename IndexType>
	void UpdateAdjacencyIndexBuffer(const TArray<IndexType>& InIndices)
	{
		AdjacencyIndexBuffer.SetData(InIndices);
	}

	TSharedPtr<FRuntimeMeshAccessor> GetSectionMeshAccessor()
	{
		return MakeShared<FRuntimeMeshAccessor>(TangentsBuffer.IsUsingHighPrecision(), UVsBuffer.IsUsingHighPrecision(), UVsBuffer.NumUVs(), IndexBuffer.Is32BitIndices(),
			&PositionBuffer.GetData(), &TangentsBuffer.GetData(), &UVsBuffer.GetData(), &ColorBuffer.GetData(), &IndexBuffer.GetData());
	}

	TUniquePtr<FRuntimeMeshScopedUpdater> GetSectionMeshUpdater(const FRuntimeMeshDataPtr& ParentData, int32 SectionIndex, int32 LODIndex, ESectionUpdateFlags UpdateFlags, FRuntimeMeshLockProvider* LockProvider, bool bIsReadonly)
	{
		return TUniquePtr<FRuntimeMeshScopedUpdater>(new FRuntimeMeshScopedUpdater(ParentData, SectionIndex, LODIndex, UpdateFlags, TangentsBuffer.IsUsingHighPrecision(), UVsBuffer.IsUsingHighPrecision(), UVsBuffer.NumUVs(), IndexBuffer.Is32BitIndices(),
			&PositionBuffer.GetData(), &TangentsBuffer.GetData(), &UVsBuffer.GetData(), &ColorBuffer.GetData(), &IndexBuffer.GetData(), LockProvider, bIsReadonly));
	}

	TSharedPtr<FRuntimeMeshIndicesAccessor> GetTessellationIndexAccessor()
	{
		return MakeShared<FRuntimeMeshIndicesAccessor>(AdjacencyIndexBuffer.Is32BitIndices(), &AdjacencyIndexBuffer.GetData());
	}

	bool CheckTangentBuffer(bool bInUseHighPrecision) const
	{
		return TangentsBuffer.IsUsingHighPrecision() == bInUseHighPrecision;
	}

	bool CheckUVBuffer(bool bInUseHighPrecision, int32 InNumUVs) const
	{
		return UVsBuffer.IsUsingHighPrecision() == bInUseHighPrecision && UVsBuffer.NumUVs() == InNumUVs;
	}

	bool CheckIndexBufferSize(bool b32BitIndices) const
	{
		return b32BitIndices == IndexBuffer.Is32BitIndices();
	}


	friend FArchive& operator <<(FArchive& Ar, FRuntimeMeshSectionLODData& LODData)
	{
		Ar << LODData.PositionBuffer;
		Ar << LODData.TangentsBuffer;
		Ar << LODData.UVsBuffer;
		Ar << LODData.ColorBuffer;

		Ar << LODData.IndexBuffer;
		Ar << LODData.AdjacencyIndexBuffer;

		return Ar;
	}
};


class FRuntimeMeshSection
{
	const EUpdateFrequency UpdateFrequency;
	
	TArray<FRuntimeMeshSectionLODData, TInlineAllocator<RUNTIMEMESH_MAXLODS>> LODs;

	FBox LocalBoundingBox;

	bool bCollisionEnabled;

	bool bIsVisible;

	bool bCastsShadow;
public:
	FRuntimeMeshSection(FArchive& Ar);
	FRuntimeMeshSection(bool bInUseHighPrecisionTangents, bool bInUseHighPrecisionUVs, int32 InNumUVs, bool b32BitIndices, EUpdateFrequency InUpdateFrequency);

	void AddLODLevelIfNotExists(int32 Index)
	{
		check(Index >= 0 && Index < RUNTIMEMESH_MAXLODS);

		while (!LODs.IsValidIndex(Index))
		{
			LODs.Emplace(LODs[0].TangentsBuffer.IsUsingHighPrecision(), LODs[0].UVsBuffer.IsUsingHighPrecision(), LODs[0].UVsBuffer.NumUVs(), LODs[0].IndexBuffer.Is32BitIndices());
		}
	}
	
	bool IsCollisionEnabled() const { return bCollisionEnabled; }
	bool IsVisible() const { return bIsVisible; }
	bool ShouldRender() const { return IsVisible() && HasValidMeshData(); }
	bool CastsShadow() const { return bCastsShadow; }
	EUpdateFrequency GetUpdateFrequency() const { return UpdateFrequency; }
	FBox GetBoundingBox() const { return LocalBoundingBox; }

	int32 GetNumVertices(int32 LODIndex) const 
	{ 
		check(LODs.IsValidIndex(LODIndex));
		return LODs[LODIndex].PositionBuffer.GetNumVertices();
	}
	int32 GetNumIndices(int32 LODIndex) const 
	{
		check(LODs.IsValidIndex(LODIndex));
		return LODs[LODIndex].IndexBuffer.GetNumIndices();
	}
	int32 GetNumLODs() const
	{
		return LODs.Num();
	}

	bool HasValidMeshData() const 
	{
		return LODs.IsValidIndex(0) && LODs[0].HasValidMeshData();
	}

	void SetVisible(bool bNewVisible)
	{
		bIsVisible = bNewVisible;
	}
	void SetCastsShadow(bool bNewCastsShadow)
	{
		bCastsShadow = bNewCastsShadow;
	}
	void SetCollisionEnabled(bool bNewCollision)
	{
		bCollisionEnabled = bNewCollision;
	}

	void UpdatePositionBuffer(int32 LODIndex, TArray<uint8>& InVertices, bool bUseMove)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdatePositionBuffer(InVertices, bUseMove);

		if (LODIndex == 0)
		{
			UpdateBoundingBox();
		}
	}

	template<typename VertexType>
	void UpdatePositionBuffer(int32 LODIndex, const TArray<VertexType>& InVertices, const FBox* BoundingBox = nullptr)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdatePositionBuffer(InVertices, bUseMove);

		if (LODIndex == 0)
		{
			if (BoundingBox)
			{
				LocalBoundingBox = *BoundingBox;
			}
			else
			{
				UpdateBoundingBox();
			}
		}
	}

	void UpdateTangentsBuffer(int32 LODIndex, TArray<uint8>& InVertices, bool bUseMove)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateTangentsBuffer(InVertices, bUseMove);
	}

	template<typename VertexType>
	void UpdateTangentsBuffer(int32 LODIndex, const TArray<VertexType>& InVertices)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateTangentsBuffer(InVertices);
	}

	void UpdateUVsBuffer(int32 LODIndex, TArray<uint8>& InVertices, bool bUseMove)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateUVsBuffer(InVertices, bUseMove);
	}

	template<typename VertexType>
	void UpdateUVsBuffer(int32 LODIndex, const TArray<VertexType>& InVertices)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateUVsBuffer(InVertices);
	}

	void UpdateColorBuffer(int32 LODIndex, TArray<uint8>& InVertices, bool bUseMove)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateColorBuffer(InVertices, bUseMove);
	}

	template<typename VertexType>
	void UpdateColorBuffer(int32 LODIndex, const TArray<VertexType>& InVertices)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateColorBuffer(InVertices);
	}

	void UpdateIndexBuffer(int32 LODIndex, TArray<uint8>& InIndices, bool bUseMove)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateIndexBuffer(InIndices, bUseMove);
	}

	template<typename IndexType>
	void UpdateIndexBuffer(int32 LODIndex, const TArray<IndexType>& InIndices)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateIndexBuffer(InIndices);
	}

	template<typename IndexType>
	void UpdateAdjacencyIndexBuffer(int32 LODIndex, const TArray<IndexType>& InIndices)
	{
		AddLODLevelIfNotExists(LODIndex);

		LODs[LODIndex].UpdateAdjacencyIndexBuffer(InIndices);
	}

	TSharedPtr<FRuntimeMeshAccessor> GetSectionMeshAccessor(int32 LODIndex)
	{
		AddLODLevelIfNotExists(LODIndex);

		return LODs[LODIndex].GetSectionMeshAccessor();
	}

	TUniquePtr<FRuntimeMeshScopedUpdater> GetSectionMeshUpdater(const FRuntimeMeshDataPtr& ParentData, int32 SectionIndex, int32 LODIndex, ESectionUpdateFlags UpdateFlags, FRuntimeMeshLockProvider* LockProvider, bool bIsReadonly)
	{
		AddLODLevelIfNotExists(LODIndex);

		return LODs[LODIndex].GetSectionMeshUpdater(ParentData, SectionIndex, LODIndex, UpdateFlags, LockProvider, bIsReadonly);
	}

	TSharedPtr<FRuntimeMeshIndicesAccessor> GetTessellationIndexAccessor(int32 LODIndex)
	{
		AddLODLevelIfNotExists(LODIndex);

		return LODs[LODIndex].GetTessellationIndexAccessor();
	}



	TSharedPtr<struct FRuntimeMeshSectionCreationParams, ESPMode::NotThreadSafe> GetSectionCreationParams();

	TSharedPtr<struct FRuntimeMeshSectionUpdateParams, ESPMode::NotThreadSafe> GetSectionUpdateData(int32 LODIndex, ERuntimeMeshBuffersToUpdate BuffersToUpdate);

	TSharedPtr<struct FRuntimeMeshSectionPropertyUpdateParams, ESPMode::NotThreadSafe> GetSectionPropertyUpdateData();

	void UpdateBoundingBox();
	void SetBoundingBox(const FBox& InBoundingBox) { LocalBoundingBox = InBoundingBox; }

	int32 GetCollisionData(int32 LODIndex, TArray<FVector>& OutPositions, TArray<FTriIndices>& OutIndices, TArray<FVector2D>& OutUVs);


	bool CheckTangentBuffer(bool bInUseHighPrecision) const
	{
		return LODs[0].TangentsBuffer.IsUsingHighPrecision() == bInUseHighPrecision;
	}

	bool CheckUVBuffer(bool bInUseHighPrecision, int32 InNumUVs) const
	{
		return LODs[0].UVsBuffer.IsUsingHighPrecision() == bInUseHighPrecision && LODs[0].UVsBuffer.NumUVs() == InNumUVs;
	}

	bool CheckIndexBufferSize(bool b32BitIndices) const
	{
		return b32BitIndices == LODs[0].IndexBuffer.Is32BitIndices();
	}

	friend FArchive& operator <<(FArchive& Ar, FRuntimeMeshSection& MeshData)
	{
		Ar << const_cast<EUpdateFrequency&>(MeshData.UpdateFrequency);
		
		if (Ar.CustomVer(FRuntimeMeshVersion::GUID) >= FRuntimeMeshVersion::AddLODSupport)
		{
			Ar << MeshData.LODs;
		}
		else
		{
			MeshData.LODs.SetNum(1);

			if (Ar.CustomVer(FRuntimeMeshVersion::GUID) >= FRuntimeMeshVersion::RuntimeMeshComponentUE4_19)
			{
				Ar << MeshData.LODs[0].PositionBuffer;
				Ar << MeshData.LODs[0].TangentsBuffer;
				Ar << MeshData.LODs[0].UVsBuffer;
				Ar << MeshData.LODs[0].ColorBuffer;
			}
			else
			{
				// This is a hack to read the old data and ignore it
				Ar << MeshData.LODs[0].PositionBuffer;
				Ar << MeshData.LODs[0].PositionBuffer;
				Ar << MeshData.LODs[0].PositionBuffer;
			}


			Ar << MeshData.LODs[0].IndexBuffer;
			Ar << MeshData.LODs[0].AdjacencyIndexBuffer;
		}

		Ar << MeshData.LocalBoundingBox;

		Ar << MeshData.bCollisionEnabled;
		Ar << MeshData.bIsVisible;
		Ar << MeshData.bCastsShadow;

		// This is a hack to read the old data and ignore it
		if (Ar.CustomVer(FRuntimeMeshVersion::GUID) < FRuntimeMeshVersion::RuntimeMeshComponentUE4_19)
		{
			TArray<FVector> NullPositions;
			TArray<uint8> NullIndices;
			MeshData.LODs[0].PositionBuffer.SetData(NullPositions);
			MeshData.LODs[0].IndexBuffer.SetData(NullIndices, false);
			MeshData.LODs[0].AdjacencyIndexBuffer.SetData(NullIndices, false);
		}

		return Ar;
	}
};




/** Smart pointer to a Runtime Mesh Section */
using FRuntimeMeshSectionPtr = TSharedPtr<FRuntimeMeshSection, ESPMode::ThreadSafe>;




FORCEINLINE static FArchive& operator <<(FArchive& Ar, FRuntimeMeshSectionPtr& Section)
{
	if (Ar.IsSaving())
	{
		bool bHasSection = Section.IsValid();
		Ar << bHasSection;
		if (bHasSection)
		{
			Ar << *Section.Get();
		}
	}
	else if (Ar.IsLoading())
	{
		bool bHasSection;
		Ar << bHasSection;
		if (bHasSection)
		{
			Section = MakeShared<FRuntimeMeshSection, ESPMode::ThreadSafe>(Ar);
		}
	}
	return Ar;
}