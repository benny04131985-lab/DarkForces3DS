#pragma once
// DF3DS_C3D4_ONEWALL1_V1_MODULE
#include <cstdint>
namespace DF3DS_OneWallC3D
{
    void beginFrame();
    // DF3DS_C3D7_OPAQUEWORLD1_V8: exact geometry + wall UV carrier.
    void rememberAcceptedSolid(const void* wallKey, std::int32_t x0View, std::int32_t z0View,
        std::int32_t x1View, std::int32_t z1View, std::int32_t floorHeight,
        std::int32_t ceilingHeight, std::int32_t projectedX0, std::int32_t projectedX1,
        std::int32_t uCoord0, std::int32_t uScale, std::int32_t orientDzDx,
        std::int32_t midOffsetX, std::int32_t midOffsetZ, std::int32_t midTexelHeight,
        std::int32_t midTextureWidth, std::int32_t flipHoriz, std::int32_t hasMidTexture);
    void considerDrawnSolid(const void* wallKey, std::int32_t drawX0, std::int32_t drawX1,
        std::int32_t eyeHeight, std::int32_t focalLength, std::int32_t focalLenAspect,
        std::int32_t projOffsetX, std::int32_t projOffsetY);
    // DF3DS_C3D6_ADJOINWALLS1_V3
    enum SurfaceClass : std::uint32_t
    {
        Surface_Solid = 0u,
        Surface_Upper = 1u,
        Surface_Lower = 2u,
        Surface_Floor = 3u,
        Surface_Ceiling = 4u
    };

    // DF3DS_C3D8_RETAILWORLD1_V1: runtime-authored material/weapon handoff; no retail bytes embedded.
    void registerWallMaterial(
        const void* wallKey, SurfaceClass surfaceClass,
        const void* textureKey, const void* image,
        std::int32_t width, std::int32_t height, std::int32_t logSizeY, std::int32_t palIndex,
        std::int32_t offsetX, std::int32_t offsetZ, std::int32_t texelHeight, std::int32_t flipHoriz);

    // DF3DS_C3D27_MIRRORPAIR1_V3: opposite-side identity for telemetry only.
    void considerDrawnAdjoinStrip(
        const void* wallKey,
        const void* mirrorKey,
        std::int32_t drawX0,
        std::int32_t drawX1,
        std::int32_t eyeHeight,
        std::int32_t focalLength,
        std::int32_t focalLenAspect,
        std::int32_t projOffsetX,
        std::int32_t projOffsetY,
        std::int32_t worldHeightA,
        std::int32_t worldHeightB,
        SurfaceClass surfaceClass);

    // DF3DS_C3D12_FIX1_V1: begin the sector from authored material/vertex state,
    // stream exact v0/v1 edges from Jedi's already-existing wall loop, then explicitly finish.
    // Actual captured edge count is authoritative; wallCount metadata is not a topology gate.
    void captureSectorFlats(
        const void* sectorKey, const void* verticesWS, const void* verticesVS,
        std::int32_t vertexCount,
        std::int32_t floorHeight, std::int32_t ceilingHeight,
        std::int32_t eyeHeight, std::int32_t focalLength, std::int32_t focalLenAspect,
        std::int32_t projOffsetX, std::int32_t projOffsetY,
        const void* floorTextureKey, const void* floorImage, std::int32_t floorWidth, std::int32_t floorHeightTex,
        std::int32_t floorLogSizeY, std::int32_t floorPalIndex, std::int32_t floorOffsetX, std::int32_t floorOffsetZ,
        const void* ceilTextureKey, const void* ceilImage, std::int32_t ceilWidth, std::int32_t ceilHeightTex,
        std::int32_t ceilLogSizeY, std::int32_t ceilPalIndex, std::int32_t ceilOffsetX, std::int32_t ceilOffsetZ);
    void captureSectorFlatEdge(const void* sectorKey, std::int32_t v0Index, std::int32_t v1Index);
    void finishSectorFlatTopology(const void* sectorKey);



    // DF3DS_C3D10_NATIVEPLANES1_V2: classic traversal/frame selection authorizes visibility,
    // but Citro3D owns billboard geometry from SecObject + WaxFrame coordinates.
    void captureNativeSpritePlane(
        const void* objKey, const void* frameKey,
        const void* cellKey, const void* cellData, const void* image, const void* columnOffsets,
        std::int32_t sizeX, std::int32_t sizeY, std::int32_t compressed, std::int32_t flip,
        std::int32_t objXWS, std::int32_t objYWS, std::int32_t objZWS,
        std::int32_t objXVS, std::int32_t objYVS, std::int32_t objZVS,
        std::int32_t frameOffsetX, std::int32_t frameOffsetY,
        std::int32_t frameWidthWS, std::int32_t frameHeightWS,
        std::int32_t focalLength, std::int32_t focalLenAspect,
        std::int32_t projOffsetX, std::int32_t projOffsetY,
        const void* lightTable, std::int32_t sourceKind);

    // DF3DS_C3D14_SPRITEV1_V3: runtime-only ownership census. These calls never alter classic selection or world state.
    void auditSectorGeometry(const void* sectorKey, const void* verticesWS, std::int32_t vertexCount,
        std::int32_t floorHeight, std::int32_t ceilingHeight);
    void auditSignSurface(const void* wallKey, const void* textureKey,
        std::int32_t offsetX, std::int32_t offsetZ, std::uint32_t flags1);
    // DF3DS_C3D18_SIGN1_V1: native wall SIGN handoff. Classic setupSignTexture remains the state/asset owner.
    void captureNativeSign(
        const void* wallKey, const void* textureKey, const void* image,
        std::int32_t width, std::int32_t height, std::int32_t logSizeY, std::int32_t palIndex,
        std::int32_t offsetX, std::int32_t offsetZ,
        std::int32_t transparent, std::int32_t illumSign);

    // DF3DS_C3D21_LOWERCLIPAUD1_V2: evidence-only parity tap after classic lower-wall vertical window clamp.
    void auditLowerVerticalClip(
        const void* wallKey, std::int32_t x,
        std::int32_t rawTop, std::int32_t rawBottom,
        std::int32_t clippedTop, std::int32_t clippedBottom,
        std::int32_t windowTop, std::int32_t windowBottom);

    // DF3DS_C3D22_MASKROUTE1_V1: classify classic mask routing and the deferred ADJ_MID_TEX software pixel pass.

    // DF3DS_C3D53_PORTALLINE1_V1: bounded classic-recursion portal lineage; telemetry only.
    void portalLineEnter(
        const void* portalWall,const void* curSector,const void* nextSector,
        std::int32_t depth,std::int32_t childX0,std::int32_t childX1,
        std::int32_t childMinY,std::int32_t childMaxY,
        std::int32_t wallX0,std::int32_t wallX1,
        std::int32_t z0,std::int32_t z1,
        std::int32_t curFloor,std::int32_t curCeil,
        std::int32_t nextFloor,std::int32_t nextCeil,
        std::uint32_t curFlags,std::uint32_t nextFlags);
    void portalLineLeave();


    // DF3DS_C3D55_OWNERTRACE1_V3 — telemetry only; no renderer ownership.
    void ownerTraceClassicTransparent(
        const void* wallKey,const void* textureKey,const void* imagePtr,
        std::uint32_t wallFlags,std::int32_t drawFlags,
        std::int32_t x0,std::int32_t x1);

    void auditMaskRoute(
        const void* wallKey, const void* textureKey, const void* nextSectorKey,
        std::int32_t drawFlags, std::int32_t drawX0, std::int32_t drawX1,
        std::int32_t sectorExterior, std::int32_t nextExtAdj, std::int32_t nextExtFloorAdj,
        std::int32_t adjMidTex, std::int32_t stage);

    void auditMaskedWall(const void* wallKey, const void* textureKey, const void* nextSectorKey);
    void audit3DObject(const void* objectKey, const void* modelKey,
        std::int32_t xWS, std::int32_t yWS, std::int32_t zWS);

    // DF3DS_C3D11_TAKEOVERFIX1_V2: exact weapon/gasmask screen blit descriptor.
    void captureWeaponBlit(
        const void* textureKey, const void* image,
        std::int32_t width, std::int32_t height, std::int32_t logSizeY, std::int32_t palIndex,
        std::int32_t screenX, std::int32_t screenY,
        std::int32_t xScale, std::int32_t yScale,
        std::int32_t clipX0, std::int32_t clipY0, std::int32_t clipX1, std::int32_t clipY1,
        const void* lightTable, std::int32_t scaled, std::int32_t lit);



    bool hasWall();
    void drawOverlay();
    void shutdown();
}
