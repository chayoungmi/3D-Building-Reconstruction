#pragma once

#include "ImageAnalyze/ImageAnalyze.h"
#include "Modeling/Face.h"
#include "Modeling/Geometry.h"

#include "FaceTexture.h"

#define MaxTextureSize 1024
#define MaxTerrainTextureSize 2048

/** �ϳ��� face, �ϳ��� EO�� ó���ϴ� texture extractor class - �ٸ� texture extractor���� ���. */
class CAbstractFaceTextureExtractor {
public:
	CAbstractFaceTextureExtractor() {}
	virtual ~CAbstractFaceTextureExtractor() {}

	/** pEO���� pFace�� �ؽ�ó�� �����Ͽ� �ؽ�ó �̹����� ����, ��ȯ�ϴ� �������̽� �Լ�.
	 */
	virtual CDib* SamplingTexture(CExteriorOrientation* pEO, CFace* pFace)=0;

protected:
	/* Name : GetColorAndSetTextureColor
	 * Process : get the color of src image (srcx, srcy) and set the color to TexImg (x,y)
	 */
//	void GetColorAndSetTextureColor(CDib* pImage, double srcx, double srcy, CDib* pTexImg, int x, int y);
};

class CBasicFaceTextureExtractor : public CAbstractFaceTextureExtractor
{
public:
	CBasicFaceTextureExtractor(void);
	virtual ~CBasicFaceTextureExtractor(void);

	//void SetFace( CFace* pFace );
	//CFace* GetFace();

	virtual CDib* SamplingTexture( CExteriorOrientation* pEO, CFace* pFace);

protected:
	/* Name : SetVirtualCamera
	 * Input : 
	 * Process : set virtual camera to extract texture
	 *Output
	 *
	 */
	void SetVirtualCamera( CExteriorOrientation* pEO, double factor=1 );

	/* Name : EstimateHomography
	 * Input : 
	 * Process : Estimate Homography using planarPos and srcCoord
	 *Output : homography Matrix
	 *
	 */
	void EstimateHomography( int vtxCount,  CVector3* planarPos,  
																	CVector2* srcCoord, double** homography);

	// virtual camera position�� dist ��ŭ �� z ������ �ָ� ����
	void RePositionVirtualCamera( CExteriorOrientation* pEO, CFace* pFace, double dist );
protected:
	CFace* m_pFace; 
	int m_vertexNum;

	// virtual camera 
	CVector3 m_virtualCamPos; // virtual camera position
	double** m_pVirtualCamMat; // projection matrix
	double** m_pInvExtVirtualMat; // inverse extrinsic matrix
	double** m_pIntVirtualCamMat; // intrinsic matrix
	double** m_pInvIntVirtualMat; // inverse intrinsic matrix
};

/** �ϳ��� EO, �ϳ��� face�� ���� visibility check�� �ϴ� texture extractor
 */
class CVisibleFaceTextureExtractor : public CBasicFaceTextureExtractor {
public:
	CVisibleFaceTextureExtractor() {}
	virtual ~CVisibleFaceTextureExtractor() {}

	virtual CDib* SamplingTexture(CExteriorOrientation* pEO, CFace* pFace, TFaceIDImageList* pFaceIDImageList=NULL, 
		CFaceTexture* pDesTexImg=NULL, bool bIsTerrain = false );

protected:

	CBBox AdjustVirtualCamera( CExteriorOrientation* pEO, CFace* pFace, CFaceTexture* pDesTexImg );
	CBBox GetBoundingBoxOfCamera( CExteriorOrientation* pEO, CFace* pFace, CFaceTexture* pDesTexImg );

	void CalculateAnlgeBtwFaceAndCameras( CFace* pFace, TFaceIDImageList* pRenderedImageList );
	void SortMostOrthoCamera( TFaceIDImageList* pFaceIDImageList );

	// �ٴ� �޽� �ؽ�ó ����
	CDib* CreateTerrainVirtualTexture( CBBox bbox, TFaceIDImageList* pFaceIDImageList );

	/* Name : EstimateBoundingBox
	 * Input : 
	 * Process : set boundingBox and planarPos, srcCoord
	 *Output
	 *
	 */
	// estimate bounding box of face texture and 2D src vertex coordinate ( if polygon is not triangle)
	void EstimateBoundingBox(double** orgPrj, CVector3* planarPos, 
 							 CVector2* pPlanarCoords, CVector2* srcCoord, CBBox* bbox, bool triangleFlag);
	CDib* CreateVirtualTexture(CBBox bbox, TFaceIDImageList* pFaceIDImageList);
	bool ExistFeaturesInImage( CFace* pFace, CExteriorOrientation* pEO );

	//�ش� x,y, z ��ǥ�� 2D ���� ���� pTexImg�� �����ϴ� �Լ�.
	bool GetColorAndSetColor( CVector3 pos, CDib* pTexImg, int x, int y, CExteriorOrientation* pMainRefEO );

	// get face color ID and compare btw face color ID and rendered Image 
	// if color Id and rendered image are same, get color EO image color and set color to point of virtual texture 
	// @param pTempTexImageList ����׿�
	bool GetColorWithVisibilityCheckAndSetColor( TFaceIDImageList* pFaceIDImageList, 
		CVector3 pos, CDib* pTexImg, int x, int y, CExteriorOrientation* pMainRefEO, std::vector<CDib*>* pTempTexImgList = NULL );

};
