// Copyright 2016-2018 Chris Conway (Koderz). All Rights Reserved.

#include "RuntimeMeshComponentProxy.h"
#include "RuntimeMeshComponentPlugin.h"
#include "RuntimeMeshComponent.h"
#include "RuntimeMeshProxy.h"
#include "PhysicsEngine/BodySetup.h"

FRuntimeMeshComponentSceneProxy::FRuntimeMeshComponentSceneProxy(URuntimeMeshComponent* Component) 
	: FPrimitiveSceneProxy(Component)
	, BodySetup(Component->GetBodySetup())
{
	//UE_LOG(RuntimeMeshLog, Log, TEXT("[FRuntimeMeshComponentSceneProxy] Creating Scene Proxy"));
	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

	check(Component->GetRuntimeMesh() != nullptr);

	RuntimeMeshProxy = Component->GetRuntimeMesh()->EnsureProxyCreated(GetScene().GetFeatureLevel());

	// Setup our material map


	for (auto SectionId : Component->GetRuntimeMesh()->GetSectionIds())
	{
		UMaterialInterface* Mat = Component->GetMaterial(SectionId);
		if (Mat == nullptr)
		{
			Mat = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		SectionRenderData.Add(SectionId, FRuntimeMeshSectionRenderData{ Mat, false });

		MaterialRelevance |= Mat->GetRelevance(GetScene().GetFeatureLevel());
	}
}

FRuntimeMeshComponentSceneProxy::~FRuntimeMeshComponentSceneProxy()
{

}

void FRuntimeMeshComponentSceneProxy::CreateRenderThreadResources()
{
	RuntimeMeshProxy->CalculateViewRelevance(bHasStaticSections, bHasDynamicSections, bHasShadowableSections);

	for (auto& Entry : SectionRenderData)
	{
		if (RuntimeMeshProxy->GetSections().Contains(Entry.Key))
		{
			FRuntimeMeshSectionProxyPtr Section = RuntimeMeshProxy->GetSections()[Entry.Key];

			auto& RenderData = SectionRenderData[Entry.Key];

			RenderData.bWantsAdjacencyInfo = RequiresAdjacencyInformation(RenderData.Material, RuntimeMeshProxy->GetSections()[Entry.Key]->GetLOD(0)->GetVertexFactory()->GetType(), GetScene().GetFeatureLevel());
		}
	}

	FPrimitiveSceneProxy::CreateRenderThreadResources();
}

FPrimitiveViewRelevance FRuntimeMeshComponentSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);

	bool bForceDynamicPath = !IsStaticPathAvailable() || IsRichView(*View->Family) || IsSelected() || View->Family->EngineShowFlags.Wireframe;
	Result.bStaticRelevance = !bForceDynamicPath && bHasStaticSections;
	Result.bDynamicRelevance = bForceDynamicPath || bHasDynamicSections;

	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

void FRuntimeMeshComponentSceneProxy::CreateMeshBatch(FMeshBatch& MeshBatch, const FRuntimeMeshSectionProxyPtr& Section, int32 LODIndex, const FRuntimeMeshSectionRenderData& RenderData, FMaterialRenderProxy* Material, FMaterialRenderProxy* WireframeMaterial) const
{
	//UE_LOG(RuntimeMeshLog, Log, TEXT("[FRuntimeMeshComponentSceneProxy] Creating mesh bath at LOD %d"), LODIndex);

	/* Needs to be set :
		bWireframe
		bRequiresAdjacencyInformation
		MeshBatch :
			MaterialRenderProxy
			+VertexFactory
			LCI								(Opt)
			+LODIndex
			+VisualizeLODIndex
			VisualizeHLODIndex				(Opt)
			+ReverseCulling					(Opt)
			CastShadow
			DepthPriorityGroup
			bDitheredLODTransition
			VertexFactory					(If override vertex color)
		BatchElement:
			VertexFactoryUserData
			UserData						(If override vertex color)
			bUserDataIsColorVertexBuffer	(If override vertex color)
			+PrimitiveUniformBufferResource
			+MinVertexIndex
			+MaxVertexIndex
			VisualizeElementIndex
			+MaxScreenSize
			+MinScreenSize
	*/

	bool bRenderWireframe = WireframeMaterial != nullptr;
	bool bWantsAdjacency = !bRenderWireframe && RenderData.bWantsAdjacencyInfo;
	   	  
	Section->GetLOD(LODIndex)->CreateMeshBatch(MeshBatch, Section->CastsShadow(), bWantsAdjacency);
	/* Sets :
		MeshBatch:
			MeshBatch.VertexFactory
			MeshBatch.Type
			MeshBatch.DepthPriorityGroup;
			MeshBatch.CastShadow;
		BatchElement :
			BatchElement.IndexBuffer;
			BatchElement.FirstIndex;
			BatchElement.NumPrimitives;
			BatchElement.MinVertexIndex;
			BatchElement.MaxVertexIndex;
	*/


	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];

	MeshBatch.LODIndex = LODIndex;
#if RUNTIMEMESH_ENABLE_DEBUG_RENDERING
	MeshBatch.VisualizeLODIndex = LODIndex;
#endif

	MeshBatch.bDitheredLODTransition = false; // !IsMovable() && Material->GetMaterialInterface()->IsDitheredLODTransition();



	MeshBatch.bWireframe = WireframeMaterial != nullptr;
	MeshBatch.MaterialRenderProxy = MeshBatch.bWireframe ? WireframeMaterial : Material;

	MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
	MeshBatch.bCanApplyViewModeOverrides = true;

	BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();

	BatchElement.MaxScreenSize = RuntimeMeshProxy->GetScreenSize(LODIndex);
	BatchElement.MinScreenSize = RuntimeMeshProxy->GetScreenSize(LODIndex + 1); 

	return;
}

void FRuntimeMeshComponentSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	//UE_LOG(RuntimeMeshLog, Log, TEXT("[FRuntimeMeshComponentSceneProxy] Drawing static elements"));
	for (const auto& SectionEntry : RuntimeMeshProxy->GetSections())
	{
		FRuntimeMeshSectionProxyPtr Section = SectionEntry.Value;
		if (SectionRenderData.Contains(SectionEntry.Key) && Section.IsValid() && Section->ShouldRender() && Section->WantsToRenderInStaticPath())
		{
			int32 NumLODs = Section->NumLODs();
			for (int32 LODIndex = 0; LODIndex < NumLODs; LODIndex++)
			{
				auto* SectionLOD = Section->GetLOD(LODIndex);
				if (SectionLOD->CanRender())
				{
					const FRuntimeMeshSectionRenderData& RenderData = SectionRenderData[SectionEntry.Key];
					FMaterialRenderProxy* Material = RenderData.Material->GetRenderProxy(false);

					FMeshBatch MeshBatch;
					CreateMeshBatch(MeshBatch, Section, LODIndex, RenderData, Material, nullptr);
					PDI->DrawMesh(MeshBatch, RuntimeMeshProxy->GetScreenSize(LODIndex));
				}
			}
		}
	}
}

int32 FRuntimeMeshComponentSceneProxy::GetLOD(const FSceneView * View) const
{
	const FBoxSphereBounds& ProxyBounds = GetBounds();
	FVector4 Origin = ProxyBounds.Origin;
	float SphereRadius = ProxyBounds.SphereRadius, FactorScale = 1.0f;
	int32 MinLOD = 0;
	const int32 NumLODs = RUNTIMEMESH_MAXLODS;
	const FSceneView& LODView = GetLODView(*View);
	const float ScreenRadiusSquared = ComputeBoundsScreenRadiusSquared(Origin, SphereRadius, LODView) * FactorScale * FactorScale * LODView.LODDistanceFactor * LODView.LODDistanceFactor;
	//UE_LOG(LogTemp, Log, TEXT("Screen radius : %f"), ScreenRadiusSquared);
	// Walk backwards and return the first matching LOD
	for (int32 LODIndex = NumLODs - 1; LODIndex >= 0; --LODIndex)
	{
		if (FMath::Square(RuntimeMeshProxy->GetScreenSize(LODIndex) * 0.5f) > ScreenRadiusSquared)
		{
			//UE_LOG(LogTemp, Log, TEXT("Using LOD %d"), LODIndex);
			return FMath::Max(LODIndex, MinLOD);
		}
	}
	return MinLOD;
}

void FRuntimeMeshComponentSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	//UE_LOG(RuntimeMeshLog, Log, TEXT("[FRuntimeMeshComponentSceneProxy] Getting dynamic mesh elements"));

	// Set up wireframe material (if needed)
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if (bWireframe)
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : nullptr,
			FLinearColor(0, 0.5f, 1.f)
		);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}

	// Iterate over sections
	for (const auto& SectionEntry : RuntimeMeshProxy->GetSections())
	{
		FRuntimeMeshSectionProxyPtr Section = SectionEntry.Value;
		if (SectionRenderData.Contains(SectionEntry.Key) && Section.IsValid() && Section->ShouldRender())
		{
			// Add the mesh batch to every view it's visible in
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					bool bForceDynamicPath = IsRichView(*Views[ViewIndex]->Family) || Views[ViewIndex]->Family->EngineShowFlags.Wireframe || IsSelected() || !IsStaticPathAvailable();

					if (bForceDynamicPath || !Section->WantsToRenderInStaticPath())
					{
						const FRuntimeMeshSectionRenderData& RenderData = SectionRenderData[SectionEntry.Key];
						FMaterialRenderProxy* Material = RenderData.Material->GetRenderProxy(IsSelected());

						int32 NumLODs = Section->NumLODs();
						for (int32 LODIndex = 0; LODIndex < NumLODs; LODIndex++)
						{
							auto* SectionLOD = Section->GetLOD(LODIndex);
							if (SectionLOD->CanRender())
							{
								const FRuntimeMeshSectionRenderData& RenderData = SectionRenderData[SectionEntry.Key];
								FMaterialRenderProxy* Material = RenderData.Material->GetRenderProxy(false);

								FMeshBatch& MeshBatch = Collector.AllocateMesh();
								CreateMeshBatch(MeshBatch, Section, LODIndex, RenderData, Material, WireframeMaterialInstance);

								Collector.AddMesh(ViewIndex, MeshBatch);
							}
						}
					}
				}
			}
		}
	}

	// Draw bounds
#if RUNTIMEMESH_ENABLE_DEBUG_RENDERING
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			// Draw simple collision as wireframe if 'show collision', and collision is enabled, and we are not using the complex as the simple
			if (ViewFamily.EngineShowFlags.Collision && IsCollisionEnabled() && BodySetup && BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple)
			{
				FTransform GeomTransform(GetLocalToWorld());
				BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(FColor(157, 149, 223, 255), IsSelected(), IsHovered()).ToFColor(true), NULL, false, false, UseEditorDepthTest(), ViewIndex, Collector);
			}

			// Render bounds
			RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
		}
	}
#endif

}
