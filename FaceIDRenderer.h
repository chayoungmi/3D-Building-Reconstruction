#pragma once

#include "Modeling/geometry.h"
#include "FaceTexture.h"
#include "ImageAnalyze/ExteriorOrientation.h"

typedef stdext::hash_map< COLORREF, CFace*>		FaceColorHashMap;

class CFaceIDRenderer
{
protected:
	// render face ID 
	HDC m_hDC;
	HGLRC m_hRC; //Rendering Context

	HBITMAP			m_hOldBitmap ;
	HBITMAP			m_hBitmap ;		// render target

	GLUtriangulatorObj* m_tess;  

public:
	// Scene을 faceID 색상을 준 후 각 입력 카메라에서 렌더링한 결과 저장
	TFaceIDImageList m_pFaceIDImageList;
	FaceColorHashMap m_faceColorHaspMap;

public:
	CFaceIDRenderer();
	virtual ~CFaceIDRenderer();

	bool CreateFaceIDImages(TEOGraphNodeHead* pGroup, MeshHashMap* pMeshHashMap);
	void CountFacePixel( MeshHashMap* pMeshHashMap  );

protected:
	bool InitResource( int width, int height );
	void DestroyMemoryRenderResource();
	void CleanResource();

	bool SetupPixelFormat();
	void InitializeTessellate();
	void SetViewMatrix( CExteriorOrientation* pEO );
	void RenderToMemory( CExteriorOrientation* pEO, MeshHashMap* pMeshHashMap );
	CDib*  TransferBITMAPToDib( const CString& name );
	void RenderMeshWithFaceID( CMesh* pMesh, CExteriorOrientation* pEO );

	CFace* FindFaceID( COLORREF refColor );
};
