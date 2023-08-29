#include "stdafx.h"

#include "Modeling/PolygonTriangulation.h"

#include "FaceTextureExtractor.h"
#include "TExtureUtility.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AMAbstractTextureExtractor
//////////////////////////////////////////////////////////////////////////////////////////////////////

CBasicFaceTextureExtractor::CBasicFaceTextureExtractor(void) : m_pFace(NULL)
{
	m_pVirtualCamMat = TAlgebraD::mat_new(4, 4);
	m_pIntVirtualCamMat = TAlgebraD::mat_new(3, 3);
	m_pInvIntVirtualMat = TAlgebraD::mat_new(3, 3);
	m_pInvExtVirtualMat = TAlgebraD::mat_new(3, 3);
}

CBasicFaceTextureExtractor::~CBasicFaceTextureExtractor(void)
{
	if (m_pVirtualCamMat != NULL)
		TAlgebraD::mat_delete(m_pVirtualCamMat, 4, 4);
	if (m_pIntVirtualCamMat != NULL)
		TAlgebraD::mat_delete(m_pIntVirtualCamMat, 3, 3);
	if (m_pInvIntVirtualMat != NULL)
		TAlgebraD::mat_delete(m_pInvIntVirtualMat, 3, 3);
	if (m_pInvExtVirtualMat != NULL)
		TAlgebraD::mat_delete(m_pInvExtVirtualMat, 3, 3);

	m_pFace = NULL;
}

CDib* CBasicFaceTextureExtractor::SamplingTexture( CExteriorOrientation* pEO, CFace* pFace )
{
	if (pEO==NULL || pFace==NULL)
		return false;

	if (pFace->GetEdgeCount() <= 2)
		return false;

	CVector2 leftTop, rightBottom;
	leftTop[0] = leftTop[1] = _DBL_MAX_;
	rightBottom[0] = rightBottom[1] = -_DBL_MAX_;

	m_pFace = pFace;

	//--- set virtualCamPos, invEtxtMat, Projection mat
	//2. 가상카메라 설치
	// TO DO : thrash ==> 이미지 로딩은 이 함수 호출전에 할 것...
	if (!pEO->IsImageLoaded()) 
		pEO->LoadImage(NULL);

	if( !pEO->IsImageLoaded() )
		pEO->RegisterK();

	SetVirtualCamera( pEO );

	CVector3* planarPos = new CVector3[m_vertexNum];
	CVector2* srcCoord = new CVector2[m_vertexNum];
	CVertex* pVtx = NULL;

	int j, i = 0;

	//3. 실제 카메라의 P matrix 가져온다.
	double** orgPrj = TAlgebraD::mat_new(4, 4);
	TAlgebraD::mat_identity( orgPrj, 4, 4 );
	TAlgebraD::mat_copy(pEO->GetProjectionMatrix(), 4, 3, orgPrj, 4, 3);

	// 4. get texture
	bool triangleFlag = true;
	CVector3 pos;

	CDib* pTexImg = NULL;
	if (m_pFace->GetEdgeCount() == 3) 
	{
		//--- estimate bounding of triangle texture's size
		CEdgeOnFaceIterator edgeIter = m_pFace->GetEdgeIterator() ;
		for ( ; !edgeIter.IsEnd() ; edgeIter++, i++ )
		{
			CHalfEdge* pEdge = *edgeIter ;
			// reproject vertex to virtual image
			pVtx = pEdge->GetEndVertex();
			//pos = pVtx->GetPos();
			pos = pVtx->GetGlobalPos();

			planarPos[i][0] = m_pVirtualCamMat[0][0]*pos[0] + m_pVirtualCamMat[0][1]*pos[1] + m_pVirtualCamMat[0][2]*pos[2] + m_pVirtualCamMat[0][3];
			planarPos[i][1] = m_pVirtualCamMat[1][0]*pos[0] + m_pVirtualCamMat[1][1]*pos[1] + m_pVirtualCamMat[1][2]*pos[2] + m_pVirtualCamMat[1][3];
			planarPos[i][2] = m_pVirtualCamMat[2][0]*pos[0] + m_pVirtualCamMat[2][1]*pos[1] + m_pVirtualCamMat[2][2]*pos[2] + m_pVirtualCamMat[2][3];

			planarPos[i][2] = 1.0 / planarPos[i][2];
			planarPos[i][0] *= planarPos[i][2];
			planarPos[i][1] *= planarPos[i][2];

			if (planarPos[i][0] < leftTop[0])
				leftTop[0] = planarPos[i][0];
			if (planarPos[i][0] > rightBottom[0])
				rightBottom[0] = planarPos[i][0];
			if (planarPos[i][1] < leftTop[1])
				leftTop[1] = planarPos[i][1];
			if (planarPos[i][1] > rightBottom[1])
				rightBottom[1] = planarPos[i][1];
		}

		// size
		// size에 맞게 좌표 재할당...
		int width = (int)pow(2.0, ceil((log10(rightBottom[0] - leftTop[0])*_INV_LOG_2_)));  
		int height = (int)pow(2.0, ceil((log10(rightBottom[1] - leftTop[1])*_INV_LOG_2_)));
		int sx = (int)(width - (rightBottom[0] - leftTop[0])) / 2.0;
		int sy = (int)(height - (rightBottom[1] - leftTop[1])) / 2.0;
		int ex = width - sx - 1;  //width / 2 + sx;
		int ey = height - sy - 1; //height / 2 + sy;

		// 텍스쳐 좌표 설정
		i = 0;
		edgeIter = m_pFace->GetEdgeIterator() ;
		for ( ; !edgeIter.IsEnd() ; edgeIter++, i++ )
		{
			CHalfEdge* pEdge = *edgeIter ;
			pEdge->SetUV(((double)sx+planarPos[i][0]-leftTop[0])/width, 
				1.0 - ((double)sy+planarPos[i][1]-leftTop[1])/height);
		}

		/* texture 생성 : texel 추출*/

		CDib* pImage = pEO->GetImage();
		pTexImg = new CDib(CSize(width, height), pImage->BitCount());

		CVector3 uvs, wPos;

		int x, y;
		double srcx, srcy, srcz;

		for (j=(int)(leftTop[1]+0.5), y=sy; y<=ey; j++, y++) 
		{
			for (i=(int)(leftTop[0]+0.5), x=sx; x<=ex; i++, x++) 
			{
				// backProject ( virtual image의 point에 해당하는 3D point 구한다 )
				uvs[0] = m_pInvIntVirtualMat[0][0]*i + m_pInvIntVirtualMat[0][1]*j + m_pInvIntVirtualMat[0][2];
				uvs[1] = m_pInvIntVirtualMat[1][0]*i + m_pInvIntVirtualMat[1][1]*j + m_pInvIntVirtualMat[1][2];
				uvs[2] = m_pInvIntVirtualMat[2][0]*i + m_pInvIntVirtualMat[2][1]*j + m_pInvIntVirtualMat[2][2];
				uvs.Normalize();

				wPos[0] = m_pInvExtVirtualMat[0][0]*uvs[0] + m_pInvExtVirtualMat[0][1]*uvs[1] + m_pInvExtVirtualMat[0][2]*uvs[2];
				wPos[1] = m_pInvExtVirtualMat[1][0]*uvs[0] + m_pInvExtVirtualMat[1][1]*uvs[1] + m_pInvExtVirtualMat[1][2]*uvs[2];
				wPos[2] = m_pInvExtVirtualMat[2][0]*uvs[0] + m_pInvExtVirtualMat[2][1]*uvs[1] + m_pInvExtVirtualMat[2][2]*uvs[2];
				wPos.Normalize();

				uvs = m_pFace->CalcIntersection(m_virtualCamPos, wPos);

				// reproject to EO image
				srcx = orgPrj[0][0]*uvs[0] + orgPrj[0][1]*uvs[1] + orgPrj[0][2]*uvs[2] + orgPrj[0][3];
				srcy = orgPrj[1][0]*uvs[0] + orgPrj[1][1]*uvs[1] + orgPrj[1][2]*uvs[2] + orgPrj[1][3];
				srcz = orgPrj[2][0]*uvs[0] + orgPrj[2][1]*uvs[1] + orgPrj[2][2]*uvs[2] + orgPrj[2][3];
				srcz = 1.0 / srcz;
				srcx *= srcz;
				srcy *= srcz;

				//--- get color in src image and set the color to texture image
				CTextureUtility::GetColorAndSetTextureColor( pImage, srcx, srcy, pTexImg, x, y );
			}
		}
	}
	// 다각형일 경우
	else 
	{
		CPolygonTriangulator* pTessellator = new CPolygonTriangulator;
		CVector2* pPlanarCoords = new CVector2[m_pFace->GetEdgeCount()];

		// TODO2: 여기 잘못되어 있었음. 구현 확인.
		i = 0 ; 
		CEdgeOnFaceIterator edgeIter = m_pFace->GetEdgeIterator() ;
		for ( ; !edgeIter.IsEnd() ; edgeIter++, i++ )
		{
			CHalfEdge* pEdge = *edgeIter ;
			// reproject vertex to virtual image
			pVtx = pEdge->GetEndVertex();
			//pos = pVtx->GetPos();
			pos = pVtx->GetGlobalPos();

			planarPos[i][0] = m_pVirtualCamMat[0][0]*pos[0] + m_pVirtualCamMat[0][1]*pos[1] + m_pVirtualCamMat[0][2]*pos[2] + m_pVirtualCamMat[0][3];
			planarPos[i][1] = m_pVirtualCamMat[1][0]*pos[0] + m_pVirtualCamMat[1][1]*pos[1] + m_pVirtualCamMat[1][2]*pos[2] + m_pVirtualCamMat[1][3];
			planarPos[i][2] = m_pVirtualCamMat[2][0]*pos[0] + m_pVirtualCamMat[2][1]*pos[1] + m_pVirtualCamMat[2][2]*pos[2] + m_pVirtualCamMat[2][3];

			planarPos[i][2] = 1.0 / planarPos[i][2];
			planarPos[i][0] *= planarPos[i][2];
			planarPos[i][1] *= planarPos[i][2];

			if (planarPos[i][0] < leftTop[0])
				leftTop[0] = planarPos[i][0];
			if (planarPos[i][0] > rightBottom[0])
				rightBottom[0] = planarPos[i][0];
			if (planarPos[i][1] < leftTop[1])
				leftTop[1] = planarPos[i][1];
			if (planarPos[i][1] > rightBottom[1])
				rightBottom[1] = planarPos[i][1];

			pPlanarCoords[i][0] = planarPos[i][0];
			pPlanarCoords[i][1] = planarPos[i][1];

			srcCoord[i][0] = orgPrj[0][0]*pos[0] + orgPrj[0][1]*pos[1] + orgPrj[0][2]*pos[2] + orgPrj[0][3];
			srcCoord[i][1] = orgPrj[1][0]*pos[0] + orgPrj[1][1]*pos[1] + orgPrj[1][2]*pos[2] + orgPrj[1][3];
			double tmp     = orgPrj[2][0]*pos[0] + orgPrj[2][1]*pos[1] + orgPrj[2][2]*pos[2] + orgPrj[2][3];

			tmp = 1.0 / tmp;
			srcCoord[i][0]  = srcCoord[i][0]  * tmp;
			srcCoord[i][1]  = srcCoord[i][1]  * tmp;
		}

		// size
		rightBottom[0] = (rightBottom[0] - leftTop[0]);    // m당 10픽셀, 텍스쳐 영역 크기
		rightBottom[1] = (rightBottom[1] - leftTop[1]);
		// size에 맞게 좌표 재할당...
		int width = (int)pow(2.0, ceil((log10(rightBottom[0])*_INV_LOG_2_)));   // 텍스쳐 이미지 크기
		int height = (int)pow(2.0, ceil((log10(rightBottom[1])*_INV_LOG_2_)));
		int sx = (int)(width - rightBottom[0]) / 2.0;
		int sy = (int)(height - rightBottom[1]) / 2.0;
		int ex = sx + ceil(rightBottom[0]) - 1;
		int ey = sy + ceil(rightBottom[1]) - 1;
		for (i=0; i<m_vertexNum; i++) {
			planarPos[i][0] = (planarPos[i][0]-leftTop[0]) + sx;
			planarPos[i][1] = (planarPos[i][1]-leftTop[1]) + sy;
		}

		// 텍스쳐 좌표 설정
		i = 0;
		edgeIter = m_pFace->GetEdgeIterator() ;
		for ( ; !edgeIter.IsEnd() ; edgeIter++, i++ )
		{
			CHalfEdge* pEdge = *edgeIter ;
			pEdge->SetUV(planarPos[i][0]/width, 1.0 - planarPos[i][1]/height);
		}


		//--- estimate  homography
		double** homography = TAlgebraD::mat_new(3, 3);
		EstimateHomography( m_vertexNum, planarPos,  srcCoord, homography);

		/* texture 생성 : texel 추출*/

		CDib* pImage = pEO->GetImage();
		pTexImg = new CDib(CSize(width, height), pImage->BitCount());

		int x, y;
		double srcx, srcy, srcz;

		for (y=sy; y<=ey; y++) 
		{
			for (x=sx; x<=ex; x++) 
			{
				srcx = homography[0][0]*x+ homography[0][1]*y + homography[0][2];
				srcy = homography[1][0]*x + homography[1][1]*y + homography[1][2];
				srcz = homography[2][0]*x + homography[2][1]*y + homography[2][2];
				srcz = 1.0 / srcz;
				srcx *= srcz;
				srcy *= srcz;

				//--- get color in src image and set the color to texture image
				CTextureUtility::GetColorAndSetTextureColor( pImage, srcx, srcy, pTexImg, x, y );
			}
		}

		//--- tessellation
		int* faceIndex = pTessellator->Triangulate(pPlanarCoords, m_pFace->GetEdgeCount());
		if (faceIndex != NULL)
			m_pFace->SetFaceIndex(faceIndex);


		delete [] pPlanarCoords;
		delete pTessellator;
		TAlgebraD::mat_delete(homography, 3, 3);
	}

	//--- memory 

	delete [] srcCoord;

	TAlgebraD::mat_delete(orgPrj, 4, 4);

	return pTexImg;
}


void CBasicFaceTextureExtractor::SetVirtualCamera( CExteriorOrientation* pEO, double factor )
{
	// pEO에서 카메라 내부인자와 위치(좌표)가 필요한다
	//--- set intrinsic parameter (camera는 -z 축을 보고 있다.)
	double** tmpK = pEO->GetIntrinsicMatrix();
	//m_pIntVirtualCamMat = TAlgebraD::mat_new(3, 3);

	TAlgebraD::mat_copy(tmpK, 3, 3, m_pIntVirtualCamMat, 3, 3);

	//--- set inverse K matrix (inverse intrinsic matrix)	
	double det;
	TAlgebraD::mat_inv(m_pIntVirtualCamMat, 3, m_pInvIntVirtualMat, &det);

	// z축
	CVector3 xAxis, yAxis;
	//CVector3 normal = m_pFace->GetNormal();   
	CVector3 normal = m_pFace->GetGlobalNormal();  

	//CVector3 pos = m_pFace->GetVertexCenter();
	CVector3 pos = m_pFace->GetGlobalCenter();

	// x 축
	CFace* pFace = m_pFace;

	m_vertexNum = pFace->GetEdgeCount() + 1 ;	// TODO2: +1 확인
	CHalfEdge* pLongest = pFace->FindLongestEdge() ;

	//CVector3 stpos = pLongest->GetStartVertex()->GetPos();
	//CVector3 epos = pLongest->GetEndVertex()->GetPos();

	CVector3 stpos = pLongest->GetStartVertex()->GetGlobalPos();
	CVector3 epos = pLongest->GetEndVertex()->GetGlobalPos();
	xAxis = epos - stpos;
	xAxis.Normalize();

	// y 축
	yAxis.Cross(normal, xAxis);
	yAxis.Normalize();

	//m_pVirtualCamMat = TAlgebraD::mat_new(4, 4);
	TAlgebraD::mat_identity(m_pVirtualCamMat, 4, 4);

	//R값 설정
	m_pVirtualCamMat[0][0] = xAxis[0];		m_pVirtualCamMat[0][1] = xAxis[1];		m_pVirtualCamMat[0][2] = xAxis[2];
	m_pVirtualCamMat[1][0] = yAxis[0];		m_pVirtualCamMat[1][1] = yAxis[1];		m_pVirtualCamMat[1][2] = yAxis[2];
	m_pVirtualCamMat[2][0] = normal[0];		m_pVirtualCamMat[2][1] = normal[1];		m_pVirtualCamMat[2][2] = normal[2];

	CVector3 realCamPos = pEO->GetCameraPosition();
	double distance = realCamPos.Distance(pos) * factor ;
	CVector3 uvs, wPos;

	m_virtualCamPos[0] = pos[0] + distance * normal[0];
	m_virtualCamPos[1] = pos[1] + distance * normal[1];
	m_virtualCamPos[2] = pos[2] + distance * normal[2];

	// --- 가상 카메라의 projection matrix 만들기
	// t = -RC
	m_pVirtualCamMat[0][3] = -(m_pVirtualCamMat[0][0] * m_virtualCamPos[0] + m_pVirtualCamMat[0][1] * m_virtualCamPos[1] + m_pVirtualCamMat[0][2] * m_virtualCamPos[2]);
	m_pVirtualCamMat[1][3] = -(m_pVirtualCamMat[1][0] * m_virtualCamPos[0] + m_pVirtualCamMat[1][1] * m_virtualCamPos[1] + m_pVirtualCamMat[1][2] * m_virtualCamPos[2]);
	m_pVirtualCamMat[2][3] = -(m_pVirtualCamMat[2][0] * m_virtualCamPos[0] + m_pVirtualCamMat[2][1] * m_virtualCamPos[1] + m_pVirtualCamMat[2][2] * m_virtualCamPos[2]);

	// invExtrMat : inverse of extrinsic matrix
	//m_pInvExtVirtualMat = TAlgebraD::mat_inv(m_pVirtualCamMat, 3, &det);
	TAlgebraD::mat_inv(m_pVirtualCamMat, 3, m_pInvExtVirtualMat, &det);

	// P = KR(X-c)
	double** KR = TAlgebraD::mat_multiply(m_pIntVirtualCamMat, 3, 3, m_pVirtualCamMat, 3, 3);
	TAlgebraD::mat_copy(KR, 3, 3, m_pVirtualCamMat, 3, 3);  

	uvs[0] = m_pIntVirtualCamMat[0][0]*m_pVirtualCamMat[0][3] + m_pIntVirtualCamMat[0][1]*m_pVirtualCamMat[1][3] + m_pIntVirtualCamMat[0][2]*m_pVirtualCamMat[2][3];
	uvs[1] = m_pIntVirtualCamMat[1][0]*m_pVirtualCamMat[0][3] + m_pIntVirtualCamMat[1][1]*m_pVirtualCamMat[1][3] + m_pIntVirtualCamMat[1][2]*m_pVirtualCamMat[2][3];
	uvs[2] = m_pIntVirtualCamMat[2][0]*m_pVirtualCamMat[0][3] + m_pIntVirtualCamMat[2][1]*m_pVirtualCamMat[1][3] + m_pIntVirtualCamMat[2][2]*m_pVirtualCamMat[2][3];
	m_pVirtualCamMat[0][3] = uvs[0];
	m_pVirtualCamMat[1][3] = uvs[1];
	m_pVirtualCamMat[2][3] = uvs[2];

	TAlgebraD::mat_delete(KR, 3, 3);
}



void CBasicFaceTextureExtractor::EstimateHomography( int vtxCount,  CVector3* planarPos,  
												CVector2* srcCoord, double** homography)
{
	double** A = TAlgebraD::mat_new(8, vtxCount*2);
	double* b = new double[vtxCount*2];
	int idx;

	for (int i=0; i<vtxCount; i++) 
	{
		idx = i * 2;
		A[idx][0] = planarPos[i][0];   // 텍스쳐 이미지 좌표
		A[idx][1] = planarPos[i][1];   
		A[idx][2] = 1.0;
		A[idx][3] = 0.0;
		A[idx][4] = 0.0;
		A[idx][5] = 0.0;
		A[idx][6] = -planarPos[i][0]*srcCoord[i][0];
		A[idx][7] = -planarPos[i][1]*srcCoord[i][0];
		b[idx] = srcCoord[i][0];            // 소스 이미지 좌표

		idx++;
		A[idx][0] = 0.0;
		A[idx][1] = 0.0;
		A[idx][2] = 0.0;
		A[idx][3] = planarPos[i][0];
		A[idx][4] = planarPos[i][1];
		A[idx][5] = 1.0;
		A[idx][6] = -planarPos[i][0]*srcCoord[i][1];
		A[idx][7] = -planarPos[i][1]*srcCoord[i][1];
		b[idx] = srcCoord[i][1];
	}

	double hm[8];
	TAlgebraD::solve_linear_system(A, 8, vtxCount*2, b, hm);

	homography[0][0] = hm[0];
	homography[0][1] = hm[1];
	homography[0][2] = hm[2];
	homography[1][0] = hm[3];
	homography[1][1] = hm[4];
	homography[1][2] = hm[5];
	homography[2][0] = hm[6];
	homography[2][1] = hm[7];
	homography[2][2] = 1.0;

	TAlgebraD::mat_delete(A, 8, vtxCount*2);
	delete [] b;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// CVisibleFaceTextureExtractor
///////////////////////////////////////////////////////////////////////////////////////////////////
CDib* CVisibleFaceTextureExtractor::SamplingTexture(CExteriorOrientation* pEO, CFace* pFace, 
													TFaceIDImageList* pFaceIDImageList, CFaceTexture* pDesTexImg, bool bIsTerrain )
{
	if (pEO==NULL || pFace==NULL || pFaceIDImageList==NULL || pDesTexImg==NULL)
		return NULL;

	m_pFace = pFace;
	if (pFace->GetEdgeCount() <= 2)
		return NULL;

	 	CMesh* pMesh = pFace->GetMesh();
	 	if (pMesh->GetBottomFace() == pFace) 
		{
	 		TRACE("바닥면의 texture를 추출하려 합니다.\n");
	 		return NULL;
	 	}
/*
	if (pFace->GetNormal()[2] < -0.5) {
		TRACE("아래로 향하는 면의 텍스쳐를 추출하려함.==> 나중에 수정할 것\n");
		return NULL;
	}*/

	//SetFace( pFace );

	//2. 가상카메라 설치
	//--- set virtualCamPos, invEtxtMat, Projection mat
	SetVirtualCamera( pEO );

	if (!pEO->IsImageLoaded()) // projection matrix가 필요하다... 이미지가 아니라..
	{
		pEO->LoadImage(NULL);
		pEO->RegisterK();
	}

	// virtual camera와 face간의 거리를 적절히 조정(texture size에 영향 줌)
	//--- virtual camera bounding box 설정
	CBBox bBox = AdjustVirtualCamera( pEO, pFace, pDesTexImg );

	// texture 생성

	//--- 모든 camera와 face와의 angle 구한다.
	CalculateAnlgeBtwFaceAndCameras( pFace, pFaceIDImageList );

	//--- angle을 sorting 한다.
	SortMostOrthoCamera( pFaceIDImageList );

	CVector2 edge;
	edge[0] = bBox.m_Min3D[0];
	edge[1] = bBox.m_Min3D[1];
	pDesTexImg->SetBoundingBoxLeftTop( edge );

	edge[0] = bBox.m_Max3D[0];
	edge[1] = bBox.m_Max3D[1];
	pDesTexImg->SetBoundingBoxRightDown( edge );

	CDib* pVirtualTexture = NULL;
	if( bIsTerrain )
	{
		//terrain 텍스처 생성
		//pVirtualTexture = CreateTerrainVirtualTexture( bBox, pFaceIDImageList ); // 이 함수는 한 장의 이미지만 사용
		pVirtualTexture = CreateVirtualTexture( bBox, pFaceIDImageList);
	}
	else
	{
		// terrain 외의 face에 대한 텍스처 생성
		pVirtualTexture = CreateVirtualTexture( bBox, pFaceIDImageList);
	}

	if (pVirtualTexture != NULL) 
	{
		pDesTexImg->SetTexture(pVirtualTexture);
		//SAFE_DELETE(pVirtualTexture);
	}

	return pVirtualTexture;
}

CBBox CVisibleFaceTextureExtractor::GetBoundingBoxOfCamera( CExteriorOrientation* pEO, CFace* pFace, CFaceTexture* pDesTexImg )
{
	// 실제 카메라의 P matrix 가져온다.
	double** orgPrj = TAlgebraD::mat_new(4, 4);
	TAlgebraD::mat_identity( orgPrj, 4, 4 );
	TAlgebraD::mat_copy(pEO->GetProjectionMatrix(), 4, 3, orgPrj, 4, 3);

		//--- virtual camera bounding box 설정
	CBBox bBox;

	pDesTexImg->m_planarPos = new CVector3[m_vertexNum]; 

	CVector2* srcCoord = NULL;

	if( m_vertexNum == 3 )
	{
		EstimateBoundingBox( orgPrj, pDesTexImg->m_planarPos, NULL, srcCoord, &bBox, true );
	}
	else
	{
		CVector2* pPlanarCoords = new CVector2[m_vertexNum];
		srcCoord = new CVector2[m_vertexNum];

		EstimateBoundingBox( orgPrj, pDesTexImg->m_planarPos, pPlanarCoords, srcCoord, &bBox, false );	

		//--- tessellation
		CPolygonTriangulator* pTessellator = new CPolygonTriangulator;
		int* faceIndex = pTessellator->Triangulate(pPlanarCoords, pFace->GetEdgeCount());
		if (faceIndex != NULL)
			pFace->SetFaceIndex(faceIndex);

		delete pTessellator;
		delete [] pPlanarCoords;
	}

	// 메모리 해제
	delete [] srcCoord;
	TAlgebraD::mat_delete(orgPrj, 4, 4);

	return bBox;
}


CBBox CVisibleFaceTextureExtractor::AdjustVirtualCamera( CExteriorOrientation* pEO, CFace* pFace, CFaceTexture* pDesTexImg )
{
	CBBox bBox = GetBoundingBoxOfCamera( pEO, pFace, pDesTexImg );
	double maxLength = bBox.GetLongestEdgeLength();

	// terrain mesh 아닐 경우 face 당 최대 1024 크기로 설정
	if( ! pFace->GetMesh()->IsTerrainMesh() )
	{
		if( maxLength > MaxTextureSize )
		{
			// virtual camera dist 재설정
			double quot = maxLength / MaxTextureSize;
			double rem = (int)maxLength % MaxTextureSize;

			double factor = quot;
			if(  rem > 0 )
			{
				factor += 1.0;
			}

			if( factor <= 0 ) factor = 1.0;

			SetVirtualCamera( pEO, factor );
			bBox = GetBoundingBoxOfCamera( pEO, pFace, pDesTexImg );
		}
	}
	// terrain mesh일 경우 최대 8192 크기로 설정
	else
	{
		if( maxLength > MaxTerrainTextureSize )
		{
			// virtual camera dist 재설정
			double quot = maxLength / MaxTerrainTextureSize;
			double rem = (int)maxLength % MaxTerrainTextureSize;

			double factor = quot;
			if(  rem > 0 )
			{
				factor += 1.0;
			}

			if( factor <= 0 ) factor = 1.0;

			SetVirtualCamera( pEO, factor );
			bBox = GetBoundingBoxOfCamera( pEO, pFace, pDesTexImg );
		}
	}
	return bBox;
}

// face와 카메라와의 angle 구한다
void CVisibleFaceTextureExtractor::CalculateAnlgeBtwFaceAndCameras( CFace* pFace, TFaceIDImageList* pFaceIDImageList )
{
	CExteriorOrientation* pEO = NULL;
	double angle, u, v;
	CVector3 dir;
	//CVector3 normal = pFace->GetNormal();
	CVector3 normal = pFace->GetGlobalNormal();
	//CVector3 at = pFace->GetVertexCenter();
	CVector3 at = pFace->GetGlobalCenter();

	int width, height;

	TFaceIDImageListItr iter = pFaceIDImageList->begin();
	for( iter; iter != pFaceIDImageList->end(); iter++)
	{
		pEO = (*iter)->m_pEO;

		// 제약 1 :  reprojection error 
		if( pEO->GetReprojectionError() <= 300.0)
		{
			width = pEO->GetImageWidth();
			height = pEO->GetImageHeight();

			pEO->Project3DtoImagePlane(at[0], at[1], at[2], u, v);

			// 제약 2 :  Face의 중점이 영상 내에 속해야 된다.
			//if( u > 0 && u < width - 1 && v > 0 && v < height -1 )
			{
				dir = pEO->InverseProject(u, v); 
				angle = acos(dir.Dot(normal));

				// features가 이미지에 모두 포함되는지 체크. (YMCHA : 지상영상에서 없는 경우 대부분)
				(*iter)->m_isAllVerticesInImage = true; //ExistFeaturesInImage(pFace, pEO);

				// 제약 3 : face와 카메라 간의 angle
				(*iter)->m_angle = angle;
				if (angle <= _PI_2_)
					(*iter)->m_isBadImage = true;
				else
					(*iter)->m_isBadImage = false;
			}
			//else
			//{
			//	(*iter)->m_isBadImage = true;
			//}
		} // end of if( reprj < 300 )
		else
		{
			(*iter)->m_isBadImage = true;
		}
	} // end of for(cameras)
}

static bool _CompareAngle( CFaceIDImageInfo* firstEO, CFaceIDImageInfo* secondEO )
{
	return firstEO->m_angle > secondEO->m_angle;
}

//face와 카메라의 angle 들을 가장 ortho한 것부터 sorting
void CVisibleFaceTextureExtractor::SortMostOrthoCamera( TFaceIDImageList* pFaceIDImageList )
{
	std::sort( pFaceIDImageList->begin(), pFaceIDImageList->end(), _CompareAngle)	;
}

void CVisibleFaceTextureExtractor::EstimateBoundingBox( double** orgPrj, CVector3* planarPos, CVector2* pPlanarCoords, 
											CVector2* srcCoord, CBBox* bbox, bool triangleFlag )
{
	/*  projective undistortion */ 
	CVertex* pVtx;
	CVector3 pos;
	int i = 0;
	double tmp = 0;
	CVector3 leftTop, rightBottom;

	leftTop[0] = leftTop[1] = _DBL_MAX_;
	rightBottom[0] = rightBottom[1] = -_DBL_MAX_;
	leftTop[2] = rightBottom[2] = 0 ;

	CHalfEdge* pEdge = m_pFace->GetHeadEdge();

	CEdgeOnFaceIterator edgeIter = m_pFace->GetEdgeIterator() ;
	for ( ; !edgeIter.IsEnd() ; edgeIter++, i++ )
	{
		CHalfEdge* pEdge = *edgeIter ;

		// reproject vertex to virtual image
		pVtx = pEdge->GetEndVertex();
		//pos = pVtx->GetPos();
		pos = pVtx->GetGlobalPos();

		planarPos[i][0] = m_pVirtualCamMat[0][0]*pos[0] + m_pVirtualCamMat[0][1]*pos[1] + m_pVirtualCamMat[0][2]*pos[2] + m_pVirtualCamMat[0][3];
		planarPos[i][1] = m_pVirtualCamMat[1][0]*pos[0] + m_pVirtualCamMat[1][1]*pos[1] + m_pVirtualCamMat[1][2]*pos[2] + m_pVirtualCamMat[1][3];
		planarPos[i][2] = m_pVirtualCamMat[2][0]*pos[0] + m_pVirtualCamMat[2][1]*pos[1] + m_pVirtualCamMat[2][2]*pos[2] + m_pVirtualCamMat[2][3];

		planarPos[i][2] = 1.0 / planarPos[i][2];
		planarPos[i][0] *= planarPos[i][2];
		planarPos[i][1] *= planarPos[i][2];

		if (planarPos[i][0] < leftTop[0])
			leftTop[0] = planarPos[i][0];
		if (planarPos[i][0] > rightBottom[0])
			rightBottom[0] = planarPos[i][0];
		if (planarPos[i][1] < leftTop[1])
			leftTop[1] = planarPos[i][1];
		if (planarPos[i][1] > rightBottom[1])
			rightBottom[1] = planarPos[i][1];

		// 다각형의 경우 vertex마다의 2D srcImage coord 구한다.( homography 위해서 )
		if( triangleFlag == false )
		{
			pPlanarCoords[i][0] = planarPos[i][0];
			pPlanarCoords[i][1] = planarPos[i][1];     // tessellation에서만 사용한다.

			// 이미지 좌표
			//--- 3D point를 pEO의 이미지 좌표로  reprojection 시켜 해당 이미지 좌표 가져옴 
			if( srcCoord != NULL)
			{
				srcCoord[i][0] = orgPrj[0][0]*pos[0] + orgPrj[0][1]*pos[1] + orgPrj[0][2]*pos[2] + orgPrj[0][3];
				srcCoord[i][1] = orgPrj[1][0]*pos[0] + orgPrj[1][1]*pos[1] + orgPrj[1][2]*pos[2] + orgPrj[1][3];
				tmp			    = orgPrj[2][0]*pos[0] + orgPrj[2][1]*pos[1] + orgPrj[2][2]*pos[2] + orgPrj[2][3];

				tmp = 1.0 / tmp;
				srcCoord[i][0]  = srcCoord[i][0]  * tmp;
				srcCoord[i][1]  = srcCoord[i][1]  * tmp;
			}

		}
	}

	// Bounding Box 저장
	
	bbox->m_Min3D = leftTop ;
	bbox->m_Max3D = rightBottom ;
}


// terrain virtual texture 생성
CDib* CVisibleFaceTextureExtractor::CreateTerrainVirtualTexture( CBBox bbox, TFaceIDImageList* pFaceIDImageList )
{
	// size에 맞게 좌표 재할당...
	int width = (int)pow(2.0, ceil((log10(bbox.m_Max3D[0] - bbox.m_Min3D[0])*_INV_LOG_2_)));  
	int height = (int)pow(2.0, ceil((log10(bbox.m_Max3D[1] - bbox.m_Min3D[1])*_INV_LOG_2_)));
	int sx = (int)(width - (bbox.m_Max3D[0] - bbox.m_Min3D[0])) / 2.0;
	int sy = (int)(height - (bbox.m_Max3D[1] - bbox.m_Min3D[1])) / 2.0;
	int ex = width - sx - 1; 
	int ey = height - sy - 1; 

	// reprojection Error가 크지 않는 EO들 중에서 face와 가장 ortho한 angle을 가지고 
	// edge vertex들이 모두 이미지에 속하는 EO 선택.
	CExteriorOrientation* pMainRefEO = NULL;

	TFaceIDImageListItr iter = pFaceIDImageList->begin();
	for( ; iter != pFaceIDImageList->end(); iter++)
	{
		if( (*iter)->m_isBadImage == false )// && (*iter)->m_isAllVerticesInImage == true )
		{
			pMainRefEO = (*iter)->m_pEO;
			break;
		}
	}

	if( pMainRefEO == NULL)
		return NULL;

	CDib* pTexImg = new CDib(CSize(width, height), pMainRefEO->GetImage()->BitCount());

	CVector3 tmp, pos;

	for (int j=(int)(bbox.m_Min3D[1]+0.5), y=sy; y<=ey; j++, y++) 
	{
		for (int i=(int)(bbox.m_Min3D[0]+0.5), x=sx; x<=ex; i++, x++) 
		{
			// backProject ( virtual image의 2D point에 해당하는 3D point 구한다 )
			pos[0] = m_pInvIntVirtualMat[0][0]*i + m_pInvIntVirtualMat[0][1]*j + m_pInvIntVirtualMat[0][2];
			pos[1] = m_pInvIntVirtualMat[1][0]*i + m_pInvIntVirtualMat[1][1]*j + m_pInvIntVirtualMat[1][2];
			pos[2] = m_pInvIntVirtualMat[2][0]*i + m_pInvIntVirtualMat[2][1]*j + m_pInvIntVirtualMat[2][2];
			pos.Normalize();

			tmp[0] = m_pInvExtVirtualMat[0][0]*pos[0] + m_pInvExtVirtualMat[0][1]*pos[1] + m_pInvExtVirtualMat[0][2]*pos[2];
			tmp[1] = m_pInvExtVirtualMat[1][0]*pos[0] + m_pInvExtVirtualMat[1][1]*pos[1] + m_pInvExtVirtualMat[1][2]*pos[2];
			tmp[2] = m_pInvExtVirtualMat[2][0]*pos[0] + m_pInvExtVirtualMat[2][1]*pos[1] + m_pInvExtVirtualMat[2][2]*pos[2];
			tmp.Normalize();

			pos = m_pFace->CalcIntersection(m_virtualCamPos, tmp);

			bool resultColorFlag = GetColorAndSetColor( pos, pTexImg, x, y, pMainRefEO );
		}
	}

	pTexImg->CreateGdiplusBitmap();
	return pTexImg;

}

CDib* CVisibleFaceTextureExtractor::CreateVirtualTexture( CBBox bbox, TFaceIDImageList* pFaceIDImageList )
{
	// size에 맞게 좌표 재할당...
	int width = (int)pow(2.0, ceil((log10(bbox.m_Max3D[0] - bbox.m_Min3D[0])*_INV_LOG_2_)));  
	int height = (int)pow(2.0, ceil((log10(bbox.m_Max3D[1] - bbox.m_Min3D[1])*_INV_LOG_2_)));
	int sx = (int)(width - (bbox.m_Max3D[0] - bbox.m_Min3D[0])) / 2.0;
	int sy = (int)(height - (bbox.m_Max3D[1] - bbox.m_Min3D[1])) / 2.0;
	int ex = width - sx - 1; 
	int ey = height - sy - 1; 

	// reprojection Error가 크지 않는 EO들 중에서 face와 가장 ortho한 angle을 가지고 
	// edge vertex들이 모두 이미지에 속하는 EO 선택.
	CExteriorOrientation* pMainRefEO = NULL;

	TFaceIDImageListItr iter = pFaceIDImageList->begin();
	for( ; iter != pFaceIDImageList->end(); iter++)
	{
		if( (*iter)->m_isBadImage == false && (*iter)->m_isAllVerticesInImage == true )
		{
			pMainRefEO = (*iter)->m_pEO;
			break;
		}
	}

	if( pMainRefEO == NULL)
		return NULL;

	CDib* pTexImg = new CDib(CSize(width, height), pMainRefEO->GetImage()->BitCount());

#ifdef _DEBUG
	std::vector<CDib*> tempTexImgList;

	//--- 각 텍스처의 사용된 영역별 이미지를 얻기 위해 개수별로 생성
	// 이때, 마지막 이미지는 오직 한장의 이미지 만으로 텍스처를 만들었을 때의 것을 저장한다.
	int numOfDebugImage = pFaceIDImageList->size() + 1;
	for( int i = 0; i <= pFaceIDImageList->size(); i++)
	{
		CDib* tmpImage = new CDib(CSize(pTexImg->Width(), pTexImg->Height()), pMainRefEO->GetImage()->BitCount()) ;
		tempTexImgList.push_back( tmpImage );
	}

#endif 

	CVector3 tmp, pos;
	//	int x, y;

	BYTE r = 0, g = 0, b = 0;

	for (int j=(int)(bbox.m_Min3D[1]+0.5), y=sy; y<=ey; j++, y++) 
	{
		for (int i=(int)(bbox.m_Min3D[0]+0.5), x=sx; x<=ex; i++, x++) 
		{
			// backProject ( virtual image의 2D point에 해당하는 3D point 구한다 )
			pos[0] = m_pInvIntVirtualMat[0][0]*i + m_pInvIntVirtualMat[0][1]*j + m_pInvIntVirtualMat[0][2];
			pos[1] = m_pInvIntVirtualMat[1][0]*i + m_pInvIntVirtualMat[1][1]*j + m_pInvIntVirtualMat[1][2];
			pos[2] = m_pInvIntVirtualMat[2][0]*i + m_pInvIntVirtualMat[2][1]*j + m_pInvIntVirtualMat[2][2];
			pos.Normalize();

			tmp[0] = m_pInvExtVirtualMat[0][0]*pos[0] + m_pInvExtVirtualMat[0][1]*pos[1] + m_pInvExtVirtualMat[0][2]*pos[2];
			tmp[1] = m_pInvExtVirtualMat[1][0]*pos[0] + m_pInvExtVirtualMat[1][1]*pos[1] + m_pInvExtVirtualMat[1][2]*pos[2];
			tmp[2] = m_pInvExtVirtualMat[2][0]*pos[0] + m_pInvExtVirtualMat[2][1]*pos[1] + m_pInvExtVirtualMat[2][2]*pos[2];
			tmp.Normalize();

			pos = m_pFace->CalcIntersection(m_virtualCamPos, tmp);

#ifndef _DEBUG
			// source -> destinate
			bool resultColorFlag = GetColorWithVisibilityCheckAndSetColor( pFaceIDImageList, pos, pTexImg, x, y, pMainRefEO );
#else
			// tempTexImgList 에 각 사용된 픽셀들을 이미지별로 저장
			GetColorWithVisibilityCheckAndSetColor( pFaceIDImageList, pos, pTexImg, x, y, pMainRefEO,  &tempTexImgList);		
#endif

		}
	}

#ifdef _DEBUG
	//저장된 이미지를 파일로 출력
	for( int i = 0; i < tempTexImgList.size(); i++ )
	{
		char buf[256];
		sprintf_s(buf, 256, "%d_debug_tex.bmp", i );

		tempTexImgList[i]->Save( buf );
	}

	// 메모리 삭제하기

	std::vector<CDib*>::iterator debugIter = tempTexImgList.begin();
	while( debugIter != tempTexImgList.end() )
	{
		try
		{
			delete *debugIter;
			debugIter++;
		}
		catch (CMemoryException* e)
		{
			e->Delete();
		}
	}
	tempTexImgList.clear();

#endif

	pTexImg->CreateGdiplusBitmap();
	return pTexImg;
}

bool CVisibleFaceTextureExtractor::ExistFeaturesInImage( CFace* pFace, CExteriorOrientation* pEO )
{
	// 모든 feature point가 이미지안에 있는지 체크
	CVertex* pVtx = NULL;
	CCorrespondence* pCorr = NULL;
	double b[4];
	CVector3 dir;
	double uvs[3];
	bool isValid = true;
	double** pmat = NULL;
	CFeaturePoint* pFP = NULL;
	CVector2 imgCoord;

	pmat = pEO->GetProjectionMatrix();

	int width = pEO->GetImageWidth();
	int height = pEO->GetImageHeight();

	int inVertexCnt = 0;

	CEdgeOnFaceIterator edgeIter = m_pFace->GetEdgeIterator() ;
	for ( ; !edgeIter.IsEnd() ; edgeIter++ )
	{
		CHalfEdge* pEdge = *edgeIter ;
		pVtx = pEdge->GetEndVertex();

//		dir = pVtx->GetPos();
		dir = pVtx->GetGlobalPos();
		b[0] = dir[0];
		b[1] = dir[1];
		b[2] = dir[2];
		b[3] = 1.0;

		TAlgebraD::mat_vect_mul(pmat, 4, 3, b, uvs);
		uvs[2] = 1.0 / uvs[2];
		uvs[0] *= uvs[2];
		uvs[1] *= uvs[2];
		
		if (uvs[0]<0 || uvs[0]>width-1 || uvs[1]<0 || uvs[1]>height-1) 
			continue;
		
		//inVertexCnt++;
	}

	// face의 edge vertex가 잘리는 경우가 많음.
	/*if( inVertexCnt >= 3 )
		isValid = true;*/

	return isValid;
}

bool CVisibleFaceTextureExtractor::GetColorWithVisibilityCheckAndSetColor( TFaceIDImageList* pFaceIDImageList, 
													CVector3 pos, CDib* pTexImg, int x, int y, CExteriorOrientation* pMainRefEO
													, std::vector<CDib*>* pTempTexImgList )
{
	BYTE orgR, orgG, orgB;

	double srcx, srcy, srcz;

	CDib* pRefImage, *pImage;
	
	CExteriorOrientation* pEO;
		
	// face Color ID
	orgR = m_pFace->m_colorID[0];
	orgG = m_pFace->m_colorID[1];
	orgB = m_pFace->m_colorID[2];

	//--- realCamera 초기화
	double** orgPrj = TAlgebraD::mat_new(4, 4);
	TAlgebraD::mat_identity( orgPrj, 4, 4 );

	//--- 가장 ortho한 카메라의 이미지부터 texture로 사용하도록 한다.
	int currentCnt = 0;
	TFaceIDImageListItr iter = pFaceIDImageList->begin();
	for( iter; iter != pFaceIDImageList->end(); iter++)
	{
		// bad image가 아닐 경우 이미지 이용하여 텍스처 생성
		if( (*iter)->m_isBadImage == false && (*iter)->m_isAllVerticesInImage == true )
		{
			pEO = (*iter)->m_pEO;
			pRefImage = (*iter)->m_pFaceIDImage;

			// get projection matrix
			TAlgebraD::mat_copy(pEO->GetProjectionMatrix(), 4, 3, orgPrj, 4, 3);

			// get EO image
			pImage = pEO->GetImage();

			// reproject to EO image
			srcx = orgPrj[0][0]*pos[0] + orgPrj[0][1]*pos[1] + orgPrj[0][2]*pos[2] + orgPrj[0][3];
			srcy = orgPrj[1][0]*pos[0] + orgPrj[1][1]*pos[1] + orgPrj[1][2]*pos[2] + orgPrj[1][3];
			srcz = orgPrj[2][0]*pos[0] + orgPrj[2][1]*pos[1] + orgPrj[2][2]*pos[2] + orgPrj[2][3];

			srcz = 1.0 / srcz;
			srcx *= srcz;
			srcy *= srcz;

			// default Color setting
			//( 텍셀이 채워지지 않으면 까맣게 보이기 때문에 임시로 ortho에서 텍셀을 디폴트 색상으로 설정)
			if( pMainRefEO == pEO)
			{
					CTextureUtility::GetColorAndSetTextureColor( pImage, srcx, srcy, pTexImg, x, y );

#ifdef _DEBUG
					// TODO0
					// orgho에서 텍셀을 디폴트 색상으로 설정할 때, 디버그용텍스처들의 마지막 이미지로 해당 이미지를 저장하도록 한다.
					CTextureUtility::GetColorAndSetTextureColor( pImage, srcx, srcy, pTempTexImgList->back(), x, y );
#endif
			}

			// 각도가 120도 미만이면 (즉, Normal에서 60도까지만 사용) 통과
			double angle = (*iter)->m_angle;
			double threshold = 3.14159245 * (double)10 / (double)18 ;
			//if ( (*iter)->m_angle <= 3.14159245 * 10 / 18 ); //2/3 )
			/*if( angle <= threshold )
			{
				continue ;	
			}*/

			//--- get face Color ID
			
			BYTE r, g, b, a;			
			int resizeX, resizeY;
			resizeX = (int)floor(srcx/4.0 + 0.5);
			resizeY = (int)floor(srcy/4.0 + 0.5);
			
			if( resizeX < pRefImage->Width() && resizeX >= 0 )
			{
				if( resizeY < pRefImage->Height() && resizeY >= 0 )
				{
					pRefImage->ReadRGBA( r, g, b, a, resizeX, resizeY );

					if( r == orgR && g == orgG && b == orgB )
					{
						CTextureUtility::GetColorAndSetTextureColor( pImage, srcx, srcy, pTexImg, x, y );

#ifdef _DEBUG
						// TODO0
						// orgho에서 텍셀을 디폴트 색상으로 설정할 때, 디버그용텍스처들의 마지막 이미지로 해당 이미지를 저장하도록 한다.
						CTextureUtility::GetColorAndSetTextureColor( pImage, srcx, srcy, pTempTexImgList->at( currentCnt ), x, y );
#endif

						TAlgebraD::mat_delete( orgPrj, 4, 4 ) ;
						return true;
					}
				} // end of if (flipX, flipY is in imageRegion of renderedImage)
			}
		} // end of if ( not bad image )

		currentCnt++;
	}

	TAlgebraD::mat_delete(orgPrj, 4, 4);

	return false;
}

//해당 x,y, z 좌표의 2D 색상 값을 pTexImg에 설정하는 함수.
bool CVisibleFaceTextureExtractor::GetColorAndSetColor( CVector3 pos, CDib* pTexImg, int x, int y, CExteriorOrientation* pMainRefEO )
{
	BYTE orgR, orgG, orgB;

	double srcx, srcy, srcz;

	CDib* pRefImage, *pImage;

	CExteriorOrientation* pEO;

	// face Color ID
	orgR = m_pFace->m_colorID[0];
	orgG = m_pFace->m_colorID[1];
	orgB = m_pFace->m_colorID[2];

	//--- realCamera 초기화
	double** orgPrj = TAlgebraD::mat_new(4, 4);
	TAlgebraD::mat_identity( orgPrj, 4, 4 );

	//
	pEO = pMainRefEO;

	// get projection matrix
	TAlgebraD::mat_copy(pEO->GetProjectionMatrix(), 4, 3, orgPrj, 4, 3);

	// get EO image
	pImage = pEO->GetImage();

	// reproject to EO image
	srcx = orgPrj[0][0]*pos[0] + orgPrj[0][1]*pos[1] + orgPrj[0][2]*pos[2] + orgPrj[0][3];
	srcy = orgPrj[1][0]*pos[0] + orgPrj[1][1]*pos[1] + orgPrj[1][2]*pos[2] + orgPrj[1][3];
	srcz = orgPrj[2][0]*pos[0] + orgPrj[2][1]*pos[1] + orgPrj[2][2]*pos[2] + orgPrj[2][3];

	srcz = 1.0 / srcz;
	srcx *= srcz;
	srcy *= srcz;

	CTextureUtility::GetColorAndSetTextureColor( pImage, srcx, srcy, pTexImg, x, y );

	TAlgebraD::mat_delete(orgPrj, 4, 4);

	return true;
}
