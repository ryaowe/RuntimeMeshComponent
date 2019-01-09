// Copyright 2016-2018 Chris Conway (Koderz). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshData.h"
#include "RuntimeMeshSectionProxy.h"

class UBodySetup;
class URuntimeMeshComponent;

/** Runtime mesh scene proxy */
class FRuntimeMeshComponentSceneProxy : public FPrimitiveSceneProxy
{
private:
	struct FRuntimeMeshSectionRenderData
	{
		UMaterialInterface* Material;
		bool bWantsAdjacencyInfo;
	};


	FRuntimeMeshProxyPtr RuntimeMeshProxy;

	TMap<int32, FRuntimeMeshSectionRenderData> SectionRenderData;

	// Reference to the body setup for rendering.
	UBodySetup* BodySetup;

	FMaterialRelevance MaterialRelevance;

	bool bHasStaticSections;
	bool bHasDynamicSections;
	bool bHasShadowableSections;

public:

	/*Constructor, copies the whole mesh data to feed to UE */
	FRuntimeMeshComponentSceneProxy(URuntimeMeshComponent* Component);

	virtual ~FRuntimeMeshComponentSceneProxy();

	void CreateRenderThreadResources() override;

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	/* Equivalent of FStaticMeshSceneProxy::GetMeshElement */
	void CreateMeshBatch(FMeshBatch& MeshBatch, const FRuntimeMeshSectionProxyPtr& Section, int32 LODIndex, const FRuntimeMeshSectionRenderData& RenderData, FMaterialRenderProxy* Material, FMaterialRenderProxy* WireframeMaterial) const;
	
	/**
	 * Draws the primitive's static elements.  This is called from the rendering thread once when the scene proxy is created.
	 * The static elements will only be rendered if GetViewRelevance declares static relevance.
	 * @param PDI - The interface which receives the primitive elements.
	 */
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;

	/** Returns the LOD that the primitive will render at for this view. */
	virtual int32 GetLOD(const FSceneView* View) const override;

	/** Gathers a description of the mesh elements to be rendered for the given LOD index, without consideration for views. */
	virtual void GetMeshDescription(int32 LODIndex, TArray<FMeshBatch>& OutMeshElements) const override;

	/* This is called by the engine to extract the mesh data that was stored inside */
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual uint32 GetMemoryFootprint(void) const
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize(void) const
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize());
	}

#if ENGINE_MAJOR_VERSION >= 4 && ENGINE_MINOR_VERSION >= 19
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
#endif
};