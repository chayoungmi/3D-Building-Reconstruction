#include "stdafx.h"

#include "FaceIDRenderer.h"

/////////////////////////////////////////////////////////////////////////////////////
// CFaceIDRenderer	
/////////////////////////////////////////////////////////////////////////////////////

CFaceIDRenderer::CFaceIDRenderer()
{
	m_hDC = NULL;
	m_hRC = NULL; 
	m_tess = NULL;

	// initialize tessellate
	InitializeTessellate();
}

CFaceIDRenderer::~CFaceIDRenderer()
{
	CleanResource();

	if (m_tess != NULL)
		gluDeleteTess(m_tess);

	// 메모리 해제
	if( m_pFaceIDImageList.size() != 0)
	{
		TFaceIDImageListItr itr = m_pFaceIDImageList.begin(); 
		while( itr != m_pFaceIDImageList.end() )
		{
			try
			{
				delete *itr;
				itr++;
			}
			catch( CMemoryException* e)
			{
				e->Delete();
			}
		}
		m_pFaceIDImageList.clear();
	}
	m_faceColorHaspMap.clear();
}

bool CFaceIDRenderer::SetupPixelFormat()
{
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof ( PIXELFORMATDESCRIPTOR ),			// Size Of This Pixel Format Descriptor
		1,											// Version Number
		PFD_DRAW_TO_BITMAP |						// Format Must Support Window
		PFD_SUPPORT_OPENGL ,						// Format Must Support OpenGL
		PFD_TYPE_RGBA,								// Request An RGBA Format
		32,											// Select Our Color Depth
		0, 0, 0, 0, 0, 0,							// Color Bits Ignored
		0,											// No Alpha Buffer
		0,											// Shift Bit Ignored
		0,											// No Accumulation Buffer
		0, 0, 0, 0,									// Accumulation Bits Ignored
		32,											// 32Bit Z-Buffer (Depth Buffer)  
		0,											// No Stencil Buffer
		0,											// No Auxiliary Buffer
		PFD_MAIN_PLANE,								// Main Drawing Layer
		0,											// Reserved
		0, 0, 0										// Layer Masks Ignored
	};

   int nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);

   if ( nPixelFormat == 0 )
   {
	   MessageBox ( NULL, "Can't Find A Suitable PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
       return false;
   }

   if ( SetPixelFormat(m_hDC, nPixelFormat, &pfd) == FALSE)
   {
	   MessageBox (NULL,  "Can't Set The PixelFormat.", "ERROR", MB_OK | MB_ICONEXCLAMATION );
       return false;
   }
   return true;
}


void	CFaceIDRenderer::CleanResource()
{
	// Memory rendering용 resource 반환
	this->DestroyMemoryRenderResource() ;

	// rendering용 resources 반환
	wglMakeCurrent( NULL, NULL ) ;
	if ( m_hRC )
	{
		wglDeleteContext( m_hRC ) ;
	}

	m_hRC = NULL ;
	m_hDC = NULL ;

}

void	CFaceIDRenderer::DestroyMemoryRenderResource()
{
	if ( m_hDC )
	{
		if ( m_hRC )
		{
			wglMakeCurrent( NULL, NULL ) ;
			wglDeleteContext( m_hRC ) ;
			m_hRC = NULL ;
		}
		
		if( m_hBitmap )
		{
			::SelectObject( m_hDC, m_hOldBitmap ) ;
			::DeleteObject( m_hBitmap ) ;
			m_hBitmap = NULL ;
			m_hOldBitmap = NULL ;

		}
		::DeleteDC( m_hDC ) ;
		m_hDC = NULL ;
	}
}
void CFaceIDRenderer::InitializeTessellate()
{
	m_tess = gluNewTess(); 
	gluTessCallback(m_tess, GLU_TESS_BEGIN,  (void (__stdcall *)(void))CustomTessBegin);   
	gluTessCallback(m_tess, GLU_TESS_VERTEX, (void (__stdcall *)(void))glVertex3dv); 
	gluTessCallback(m_tess, GLU_TESS_END,	 (void (__stdcall *)(void))CustomTessEnd);   
	gluTessCallback(m_tess, GLU_TESS_ERROR,  (void (__stdcall *)(void))CustomTessError); 
	gluTessCallback(m_tess, GLU_TESS_COMBINE, (void (__stdcall *)(void))CustomTessVertexUVCombine);

}

bool CFaceIDRenderer::InitResource( int width, int height )
{
	HDC hDC = CreateCompatibleDC( NULL );
	
	HBITMAP hBitmap = CreateBitmap( width, height, 4, 8, NULL ) ; 
	HBITMAP hOldBitmap = (HBITMAP) SelectObject( hDC, hBitmap ) ;  
	
	if ( !hDC || !hBitmap || !hOldBitmap )
	{
		AfxMessageBox( "InitResource failed" ) ;
		return false;
	}

	m_hDC = hDC ;
	m_hBitmap = hBitmap ;
	m_hOldBitmap = hOldBitmap ;
	
	if( SetupPixelFormat() == false )
	{
		MessageBox( NULL, "Error Setup Pixel format", "ERROR", MB_OK);
		return false;
	}

	// create rendering context
	HGLRC hRC = wglCreateContext( hDC );

	if( hRC == NULL)
	{
		MessageBox( NULL, "Error creating rendering context", "ERROR", MB_OK);
		return false;
	}

	m_hRC = hRC;

	 //Make the RC Current
//     if( wglMakeCurrent (m_hDC, m_hRC)==FALSE )
//     {
// 		wglMakeCurrent( NULL, NULL );
// 		wglDeleteContext( m_hRC );
// 
//         MessageBox(NULL, "Error making RC Current", "ERROR", MB_OK);
//         return false;
//     }
	return true ;
}


void CFaceIDRenderer::SetViewMatrix( CExteriorOrientation* pEO )
{
	//====== The depth of the Z-buffer will be taken into account
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable( GL_COLOR_MATERIAL ) ;

	//====== set the background
	glClearColor( 1.0f, 1.0f, 1.0f, 0.0f );
	glClearDepth( 1.0f ) ;
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_ACCUM_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

	//--- disable texture and other effects
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_FOG);
    glDisable(GL_LIGHTING);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	GLdouble frustum[4];
	GLdouble intrinsic[4];
	GLdouble _near, _far;

	_near=1;   // 10
	_far=100000000;

	if( pEO != NULL)
	{
		// set intrinsic camera matrix
		intrinsic[0] = pEO->m_pKMtx[0][0]/4.0;
		intrinsic[1] = pEO->m_pKMtx[1][1]/4.0;
		intrinsic[2] = pEO->m_pKMtx[0][2]/4.0;
		intrinsic[3] = pEO->m_pKMtx[1][2]/4.0;

		// calc frustum size
		CDib* pImg = pEO->GetImage();

		int width = pImg->Width()/ 4.0;
		int height = pImg->Height()/ 4.0;

		GLdouble fx = intrinsic[0];
		GLdouble fy = intrinsic[1];
		GLdouble ox = intrinsic[2];
		GLdouble oy = height - intrinsic[3] ; 

		GLdouble left = - ox * _near / fx;
		GLdouble right = _near/fx * ( width - ox );
		GLdouble bottom = ( _near / fy ) * -oy;
		GLdouble top = _near / fy * ( height - oy );

		/*CDib* pImg = pEO->GetImage();
		GLdouble fx = intrinsic[0];
		GLdouble fy = intrinsic[1];
		GLdouble ox = intrinsic[2];
		GLdouble oy = (pImg->Height()/4.0) -1 - intrinsic[3] ; 

		GLdouble left = - ox * _near / fx;
		GLdouble right = (1.0 - ((double)pImg->Width()/4.0)/ox ) * left;
		GLdouble bottom = (_near * - oy) / fy;
		GLdouble top = ( 1.0 - ((double)(pImg->Height()/4.0)/oy ) ) * bottom;*/

		frustum[0] = left;
		frustum[1] = right;
		frustum[2] = bottom;
		frustum[3] = top;

		// 	set view point and set frustum
		glFrustum(frustum[0],frustum[1],frustum[2],frustum[3], _near, _far);
		glViewport( 0, 0, width, height );
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

}

void CFaceIDRenderer::RenderMeshWithFaceID( CMesh* pMesh, CExteriorOrientation* pEO )
{
	glPushMatrix();	

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// rotate( opengl에서는 카메라가 -Z 값을 바라보므로 +Z 로 변경 )
	glRotated( 180, 1, 0, 0 );

	int num_vertex;
	GLenum beginMode;

	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT, GL_FILL);
	
	//--- camera setting ( model view 이므로 inverse Extrinsic.. )

	// R 
	double** extrinsic = pEO->GetExtrinsicMatrix();

	GLdouble m[16];

	m[0] = extrinsic[0][0];	m[4] = extrinsic[0][1];	m[8] = extrinsic[0][2];	m[12] = extrinsic[0][3];
	m[1] = extrinsic[1][0];	m[5] = extrinsic[1][1];	m[9] = extrinsic[1][2];	m[13] = extrinsic[1][3];
	m[2] = extrinsic[2][0];	m[6] = extrinsic[2][1];	m[10] = extrinsic[2][2];	m[14] = extrinsic[2][3];
	m[3] = 0;					m[7] = 0;					m[11 ] = 0;					m[15] = 1;

	glMultMatrixd( m );

	//--- model draw..
	
	CVertex* pVertex = NULL;
	double* posarr = NULL;

	FaceHashMap*	pFaceHashMap = pMesh->GetFaceHashMap();
	FaceHashMap::iterator faceIter = pFaceHashMap->begin();
	for ( ; faceIter != pFaceHashMap->end(); faceIter++ )
	{
		CFace* pFace = faceIter->second;
		CHalfEdge* pEdge = NULL;
	
		num_vertex = pFace->GetEdgeCount();
		//CVector3& normal = pFace->GetNormal();
		CVector3& normal = pFace->GetGlobalNormal();

		glColor3f( pFace->m_colorID[0]/255.0, pFace->m_colorID[1]/255.0, pFace->m_colorID[2]/255.0 ); 

		if( num_vertex > 4 ) // tessellation
		{

			//-----------------------------------------------------
			//  ETRI tessellate 사용

			int* faceIndex = pFace->GetFaceIndex();
			if (faceIndex == NULL) 
			{
				TRACE("n-gon의 tessellation 정보가 없음\n");
				faceIndex = pFace->Tessellate();
			}

			glBegin(GL_TRIANGLES);
			glNormal3d(normal[0], normal[1], normal[2]);
			for (int i=0; i< (pFace->GetEdgeCount()-2)*3; i++) 
			{
				// faceIndex[i] 번째 edge를 가져온다.
				CEdgeOnFaceIterator edgeIter = pFace->GetEdgeIterator() ;
				for ( int j = 0 ; j < faceIndex[i] && !edgeIter.IsEnd() ; j++ ) edgeIter++ ;
				ASSERT( !edgeIter.IsEnd() ) ;

				if ( !edgeIter.IsEnd() )
				{
					pEdge = *edgeIter ;
					pVertex = pEdge->GetEndVertex();
					//glVertex3dv( pVertex->m_pos.GetElement() ) ;
					CVector3 pos = pVertex->GetGlobalPos();
					glVertex3dv( pos.GetElement() );
				}
				else
				{
					// 대응책 없음
				}
			}

			glEnd();

			//--------------------------
			// gluTessellate 사용
			/*
			gluTessProperty(m_tess, GLU_TESS_BOUNDARY_ONLY, false);
			gluTessBeginPolygon(m_tess, NULL);       
			gluTessBeginContour(m_tess);        
			//gluNextContour(m_tess, GLU_CCW ) ;
			gluNextContour(m_tess, GLU_EXTERIOR);

			glNormal3d(normal[0], normal[1], normal[2]); 

			CEdgeOnFaceIterator edgeIter = pFace->GetEdgeIterator() ;
			for ( ; !edgeIter.IsEnd() ; edgeIter++ )
			{
				CHalfEdge* pEdge = *edgeIter ;
				pVertex = pEdge->GetEndVertex();
				posarr = pVertex->GetPos().GetElement() ;
				gluTessVertex(m_tess, posarr, posarr);
			}

			gluTessEndContour(m_tess); 
			gluTessEndPolygon(m_tess);  
			*/
			//
			//----------------------------------------------------------

			GLenum err = glGetError() ;
			if ( err != GL_NO_ERROR )
			{
				AfxMessageBox( "GL Error after Render to memory" ) ;
			}	
		} // end of if(  over 3 )
		else
		{
			if( num_vertex == 3 )
			{
				beginMode = GL_TRIANGLES;
			}
			else if( num_vertex == 4 )
			{
				beginMode = GL_QUADS;
			}
			
			glBegin(beginMode);

			glNormal3d(normal[0], normal[1], normal[2]); 
			CEdgeOnFaceIterator edgeIter = pFace->GetEdgeIterator() ;
			for ( ; !edgeIter.IsEnd() ; edgeIter++ )
			{
				CHalfEdge* pEdge = *edgeIter ;
				pVertex = pEdge->GetEndVertex();

				CVector3 pos = pVertex->GetGlobalPos();
				glVertex3dv( pos.GetElement() );	
				
			} // end of while(pEdge) 

			glEnd();

			GLenum err = glGetError() ;
			if ( err != GL_NO_ERROR )
			{
				AfxMessageBox( "GL Error after Render to memory" ) ;
			}				
		} // end of else ( vertex cnt == 3 )
	} // end of while(pFace )

	GLenum err = glGetError() ;
	if ( err != GL_NO_ERROR )
	{
		AfxMessageBox( "GL Error after Render to memory" ) ;
	}

	glPopMatrix();	
}

void CFaceIDRenderer::RenderToMemory( CExteriorOrientation* pEO,  MeshHashMap* pMeshHashMap )
{	
	//--- pEO 사이즈에 맞게 memDC 생성
	DestroyMemoryRenderResource();
	InitResource( (int)ceil(pEO->GetImage()->Width()/4.0), (int)ceil(pEO->GetImage()->Height()/4.0) );

	//--- render 
	BOOL bResult = wglMakeCurrent( m_hDC,  m_hRC);
	if (!bResult) 
	{
		wglMakeCurrent( NULL, NULL );
		wglDeleteContext( m_hRC );
		TRACE("wglMakeCurrent Failed %x\r\n", GetLastError() ) ;
	}

	//... set cameraz
	SetViewMatrix(pEO);
	
	MeshHashMap::iterator meshIter = pMeshHashMap->begin();
	for ( ; meshIter != pMeshHashMap->end(); meshIter++ )
	{
		CMesh* pMesh = meshIter->second;
		//... set mesh
		RenderMeshWithFaceID( pMesh, pEO );
	}
	glFlush();

	GLenum err = glGetError() ;
	if ( err != GL_NO_ERROR )
	{
		AfxMessageBox( "GL Error after Render to memory" ) ;
	}	

	wglMakeCurrent( m_hDC,  NULL);

}

CDib* CFaceIDRenderer::TransferBITMAPToDib( const CString& name )
{
	//--- set resources
	HBITMAP hOldBitmap = (HBITMAP) ::SelectObject( m_hDC, m_hOldBitmap ) ;

	ASSERT( hOldBitmap == m_hBitmap ) ;

	//--- CDib 이미지로 저장

	//  BITMAP Header 정보를 가져온다.
	BITMAP bitmapData ;

	if ( !GetObject( m_hBitmap, sizeof( bitmapData ), &bitmapData ) )
	{
		ASSERT( !"Cannot get bitmap data" ) ;
		return NULL;
	}

	typedef struct
	{
		BITMAPINFOHEADER header ;
		DWORD colorMask[3] ;
	} BitFieldBitmapInfo_t ;

	BitFieldBitmapInfo_t bitmapInfo ;
	memset( &bitmapInfo, 0, sizeof( bitmapInfo ) ) ;
	bitmapInfo.header.biSize = sizeof( bitmapInfo ) ;

	HGDIOBJ hOldObj = ::SelectObject( m_hDC, m_hOldBitmap ) ;
	if ( !GetDIBits( m_hDC, m_hBitmap, 0, 0, NULL, (BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS ) )
	{
		AfxMessageBox( "GetDIBits() failed (1)" ) ;
		return NULL;
	}

	CDib* pImage = new CDib( CSize(bitmapData.bmWidth, bitmapData.bmHeight ), bitmapInfo.header.biBitCount );
	
	// BITMAP 데이터를 가져온다.
	
	int nReadLines = GetDIBits( m_hDC, m_hBitmap, 0, bitmapData.bmHeight, pImage->GetData(), 
													(BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS ) ; 
	if ( nReadLines == 0 )
	{
		AfxMessageBox( "GetDIBits failed" ) ;
		return NULL;
	}

	//// test rendered image
#ifdef _DEBUG
	char outName[64];
	sprintf_s( outName, 64, "rendered_%s.bmp", name);
	pImage->Save( outName );
	//
#endif

	hOldBitmap = (HBITMAP) ::SelectObject( m_hDC, hOldBitmap ) ;
	m_hOldBitmap = hOldBitmap ;

	return pImage;

}

/** face ID를 컬러로 하는 이미지를 생성한다.
 *  @param pGroup 렌더링 타겟의 리스트
 *  @param pMeshList 렌더링 대상인 메쉬 리스트
 *  @return 렌더링 성공여부
 */
bool CFaceIDRenderer::CreateFaceIDImages(TEOGraphNodeHead* pGroup, MeshHashMap* pMeshHashMap )
{
	if (pGroup==NULL || pMeshHashMap==NULL)
		return false;

	HDC hOldDC = wglGetCurrentDC();
	HGLRC hOldGlrc = wglGetCurrentContext();

	glPushAttrib(GL_ALL_ATTRIB_BITS );

	//--- get realCameras and Set ViewMatrix

	//... ortho camera 
	CExteriorOrientation* pEO = pGroup->GetContent();

	// 이미지 존재하는지 체크
	if (!pEO->IsImageLoaded()) 
	{
		pEO->LoadImage(NULL);
		pEO->RegisterK();
	}

	RenderToMemory( pEO, pMeshHashMap );
	CDib* pImage = TransferBITMAPToDib( pEO->GetImage()->GetFileName() );

	if( pImage != NULL)
	{
		CFaceIDImageInfo* pFaceIDImage = new CFaceIDImageInfo();
		pFaceIDImage->m_pFaceIDImage = pImage;
		pFaceIDImage->m_pEO = pEO;

		m_pFaceIDImageList.push_back(pFaceIDImage);
	}

	//.. oblique camera	
	//TEOGraphNode* pNode = pSU->GetEOGroup()->GetStartNode();
	TEOGraphNodeIterator gitr(*pGroup);
	while (!gitr.IsEnd()) 
	{
		pEO = (*gitr)->GetNodeElement();
		
		// 이미지 있는지 체크
		if (!pEO->IsImageLoaded()) 
		{
			pEO->LoadImage(NULL);
			pEO->RegisterK();
		}
	
		//pNode = pNode->GetNext();
		gitr++;

		RenderToMemory( pEO, pMeshHashMap );
		
		pImage = TransferBITMAPToDib( pEO->GetImage()->GetFileName() );
		if( pImage != NULL)
		{
			CFaceIDImageInfo* pFaceIDImage = new CFaceIDImageInfo();
			pFaceIDImage->m_pFaceIDImage = pImage;
			pFaceIDImage->m_pEO = pEO;

			m_pFaceIDImageList.push_back(pFaceIDImage);
		}
	}

	glPopAttrib();

	wglMakeCurrent( hOldDC, hOldGlrc );

	return true;
}

void CFaceIDRenderer::CountFacePixel( MeshHashMap* pMeshHashMap  )
{
	if( ! m_pFaceIDImageList.size() ) return;
	
	if ( pMeshHashMap == NULL ) return ;

	//  face 전체 개수 & color ID hash map 생성
	int totalFaceCnt = 0;
	MeshHashMap::iterator meshIter = pMeshHashMap->begin();
	for ( ; meshIter != pMeshHashMap->end(); meshIter++ )
	{
		CMesh* pMesh = meshIter->second;
		FaceHashMap* pFaceHashMap = pMesh->GetFaceHashMap();
		
		totalFaceCnt += pFaceHashMap->size();

		FaceHashMap::iterator faceItr = pFaceHashMap->begin();
		for( ; faceItr != pFaceHashMap->end(); faceItr++ )
		{
			CFace* pFace = faceItr->second;

			COLORREF colorID;
			colorID = RGB( pFace->m_colorID[0], pFace->m_colorID[1], pFace->m_colorID[2] );

			m_faceColorHaspMap.insert( FaceColorHashMap::value_type( colorID, pFace ) );
		}
	}

	// 각 이미지 마다 모든 face의 면적 저장

	TFaceIDImageListItr itr = m_pFaceIDImageList.begin(); 
	while( itr != m_pFaceIDImageList.end() )
	{
		// 면적 저장 장소 생성
		CFaceIDImageInfo* pInfo = (*itr);
		pInfo->m_faceAreaList = new unsigned int[totalFaceCnt];
		for( int i = 0; i < totalFaceCnt; i++ )
		{
			pInfo->m_faceAreaList[i] = 0;
		}

		// 면적 저장
		BYTE r, g, b ;
		CDib* pImage = pInfo->m_pFaceIDImage;
		
		for( int y = 0; y < pImage->Height(); y++ )
		{
			for( int x = 0; x < pImage->Width(); x++ )
			{
				pImage->SamplingRGB( x, y, r, g, b );
				
				COLORREF refColor;
				refColor = RGB( r, g, b );
				CFace* pFace =  FindFaceID( refColor );
				
				if( pFace == NULL ) continue;

				unsigned int faceID = pFace->GetID();
				pInfo->m_faceAreaList[faceID]++;
			}
		}	// end of for rendered image
		itr++;
	} // end of while
}

CFace* CFaceIDRenderer::FindFaceID( COLORREF refColor )
{
	FaceColorHashMap::iterator findIter = m_faceColorHaspMap.find( refColor );

	if( findIter == m_faceColorHaspMap.end() ) return NULL;
	return (*findIter).second;

}