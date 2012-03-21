//Copyright (C) 2010-2011 by Jason L. McKesson
//This file is licensed under the MIT License.


#include <string>
#include <vector>
#include <map>
#include <utility>
#include <fstream>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <glload/gl_3_2_comp.h>
#include <glload/gll.h>
#include <GL/freeglut.h>
#include <tinyxml.h>
#include "framework.h"
#include "Mesh.h"
#include "directories.h"


namespace Framework
{
	struct RenderCmd
	{
		bool bIsIndexedCmd;
		GLenum ePrimType;
		GLuint start;
		GLuint elemCount;
		GLenum eIndexDataType;	//Only if bIsIndexedCmd is true.
		int primRestart;		//Only if bIsIndexedCmd is true.

		void Render()
		{
			if(bIsIndexedCmd)
				glDrawElements(ePrimType, elemCount, eIndexDataType, (void*)start);
			else
				glDrawArrays(ePrimType, start, elemCount);
		}
	};

	union AttribData
	{
		float fValue;
		GLuint uiValue;
		GLint iValue;
		GLushort usValue;
		GLshort sValue;
		GLubyte ubValue;
		GLbyte bValue;
	};

	struct PrimitiveType
	{
		const char *strPrimitiveName;
		GLenum eGLPrimType;
	};

	struct AttribType
	{
		const char *strNameFromFile;
		bool bNormalized;
		GLenum eGLType;
		int iNumBytes;
		void(*ParseFunc)(std::vector<AttribData> &, std::istream &);
		void(*WriteToBuffer)(GLenum, const std::vector<AttribData> &, int, size_t);
	};

#define PARSE_ARRAY_FUNCDEF(attribDataValue, funcName)\
	void funcName(std::vector<AttribData> &outputData, std::istream &inStream)\
	{\
	inStream.seekg(0, std::ios_base::beg);\
	inStream >> std::skipws;\
	\
	while(!inStream.eof() && inStream.good())\
	{\
	AttribData theValue;\
	inStream >> theValue.attribDataValue >> std::ws;\
	if(inStream.fail())\
	throw std::runtime_error("Parse error in array data stream.");\
	outputData.push_back(theValue);\
	}\
	}\

	void WriteFloatsToBuffer(GLenum eBuffer, const std::vector<AttribData> &theData,
		int iSize, size_t iOffset)
	{
		std::vector<float> tempBuffer;
		tempBuffer.reserve(theData.size());

		for(size_t iLoop = 0; iLoop < theData.size(); iLoop++)
			tempBuffer.push_back(theData[iLoop].fValue);

		glBufferSubData(eBuffer, iOffset, tempBuffer.size() * sizeof(float), &tempBuffer[0]);
	}

#define WRITE_ARRAY_FUNCDEF(attribDataValue, attribType, funcName) \
	void funcName(GLenum eBuffer, const std::vector<AttribData> &theData, \
	int iSize, size_t iOffset) \
	{ \
	std::vector<attribType> tempBuffer; \
	tempBuffer.reserve(theData.size()); \
	\
	for(size_t iLoop = 0; iLoop < theData.size(); iLoop++) \
	tempBuffer.push_back(theData[iLoop].attribDataValue); \
	\
	glBufferSubData(eBuffer, iOffset, tempBuffer.size() * sizeof(attribType), &tempBuffer[0]); \
	} \


	PARSE_ARRAY_FUNCDEF(fValue,		ParseFloats);
	PARSE_ARRAY_FUNCDEF(uiValue,	ParseUInts);
	PARSE_ARRAY_FUNCDEF(iValue,		ParseInts);
	PARSE_ARRAY_FUNCDEF(usValue,	ParseUShorts);
	PARSE_ARRAY_FUNCDEF(sValue,		ParseShorts);
	PARSE_ARRAY_FUNCDEF(ubValue,	ParseUBytes);
	PARSE_ARRAY_FUNCDEF(bValue,		ParseBytes);

	WRITE_ARRAY_FUNCDEF(fValue,		float,		WriteFloats);
	WRITE_ARRAY_FUNCDEF(uiValue,	GLuint,		WriteUInts);
	WRITE_ARRAY_FUNCDEF(iValue,		GLint,		WriteInts);
	WRITE_ARRAY_FUNCDEF(usValue,	GLushort,	WriteUShorts);
	WRITE_ARRAY_FUNCDEF(sValue,		GLshort,	WriteShorts);
	WRITE_ARRAY_FUNCDEF(ubValue,	GLubyte,	WriteUBytes);
	WRITE_ARRAY_FUNCDEF(bValue,		GLbyte,		WriteBytes);



	namespace
	{
		const AttribType g_allAttributeTypes[] =
		{
			{"float",		false,	GL_FLOAT,			sizeof(GLfloat),	ParseFloats,	WriteFloats},
			{"half",		false,	GL_HALF_FLOAT,		sizeof(GLhalfARB),	ParseFloats,	WriteFloats},
			{"int",			false,	GL_INT,				sizeof(GLint),		ParseInts,		WriteInts},
			{"uint",		false,	GL_UNSIGNED_INT,	sizeof(GLuint),		ParseUInts,		WriteUInts},
			{"norm-int",	true,	GL_INT,				sizeof(GLint),		ParseInts,		WriteInts},
			{"norm-uint",	true,	GL_UNSIGNED_INT,	sizeof(GLuint),		ParseUInts,		WriteUInts},
			{"short",		false,	GL_SHORT,			sizeof(GLshort),	ParseShorts,	WriteShorts},
			{"ushort",		false,	GL_UNSIGNED_SHORT,	sizeof(GLushort),	ParseUShorts,	WriteUShorts},
			{"norm-short",	true,	GL_SHORT,			sizeof(GLshort),	ParseShorts,	WriteShorts},
			{"norm-ushort",	true,	GL_UNSIGNED_SHORT,	sizeof(GLushort),	ParseUShorts,	WriteUShorts},
			{"byte",		false,	GL_BYTE,			sizeof(GLbyte),		ParseBytes,		WriteBytes},
			{"ubyte",		false,	GL_UNSIGNED_BYTE,	sizeof(GLubyte),	ParseUBytes,	WriteUBytes},
			{"norm-byte",	true,	GL_BYTE,			sizeof(GLbyte),		ParseBytes,		WriteBytes},
			{"norm-ubyte",	true,	GL_UNSIGNED_BYTE,	sizeof(GLubyte),	ParseUBytes,	WriteUBytes},
		};

		const PrimitiveType g_allPrimitiveTypes[] =
		{
			{"triangles", GL_TRIANGLES},
			{"tri-strip", GL_TRIANGLE_STRIP},
			{"tri-fan", GL_TRIANGLE_FAN},
			{"lines", GL_LINES},
			{"line-strip", GL_LINE_STRIP},
			{"line-loop", GL_LINE_LOOP},
			{"points", GL_POINTS},
		};
	}

	struct AttribTypeFinder
	{
		typedef std::string first_argument_type;
		typedef AttribType second_argument_type;
		typedef bool result_type;

		bool operator() (const std::string &compareString, const AttribType &attrib) const
		{
			return compareString == attrib.strNameFromFile;
		}

	};

	struct PrimitiveTypeFinder
	{
		typedef std::string first_argument_type;
		typedef PrimitiveType second_argument_type;
		typedef bool result_type;

		bool operator() (const std::string &compareString, const PrimitiveType &prim) const
		{
			return compareString == prim.strPrimitiveName;
		}

	};

	const AttribType *GetAttribType(const std::string &strType)
	{
		int iArrayCount = ARRAY_COUNT(g_allAttributeTypes);
		const AttribType *pAttrib = std::find_if(
			g_allAttributeTypes, &g_allAttributeTypes[iArrayCount], std::bind1st(AttribTypeFinder(), strType));

		if(pAttrib == &g_allAttributeTypes[iArrayCount])
			throw std::runtime_error("Unknown 'type' field.");

		return pAttrib;
	}

	struct Attribute
	{
		Attribute()
			: iAttribIx(0xFFFFFFFF)
			, pAttribType(NULL)
			, iSize(-1)
			, bIsIntegral(false)
		{}

		explicit Attribute(const TiXmlElement *pAttribElem)
		{
			int iAttributeIndex;
			if(pAttribElem->QueryIntAttribute("index", &iAttributeIndex) != TIXML_SUCCESS)
				throw std::runtime_error("Missing 'index' attribute in an 'attribute' element.");
			if(!((0 <= iAttributeIndex) && (iAttributeIndex < 16)))
				throw std::runtime_error("Attribute index must be between 0 and 16.");
			iAttribIx = iAttributeIndex;

			int iVectorSize;
			if(pAttribElem->QueryIntAttribute("size", &iVectorSize) != TIXML_SUCCESS)
				throw std::runtime_error("Missing 'size' attribute in an 'attribute' element.");
			if(!((1 <= iVectorSize) && (iVectorSize < 5)))
				throw std::runtime_error("Attribute size must be between 1 and 4.");
			iSize = iVectorSize;

			std::string strType;
			if(pAttribElem->QueryStringAttribute("type", &strType) != TIXML_SUCCESS)
				throw std::runtime_error("Missing 'type' attribute in an 'attribute' element.");

			pAttribType = GetAttribType(strType);

			std::string strIntegral;
			if(pAttribElem->QueryStringAttribute("integral", &strIntegral) != TIXML_SUCCESS)
				bIsIntegral = false;
			else
			{
				if(strIntegral == "true")
					bIsIntegral = true;
				else if(strIntegral == "false")
					bIsIntegral = false;
				else
					throw std::runtime_error("Incorrect 'integral' value for the 'attribute'.");

				if(pAttribType->bNormalized)
					throw std::runtime_error("Attribute cannot be both 'integral' and a normalized 'type'.");

				if(pAttribType->eGLType == GL_FLOAT ||
					pAttribType->eGLType == GL_HALF_FLOAT ||
					pAttribType->eGLType == GL_DOUBLE)
					throw std::runtime_error("Attribute cannot be both 'integral' and a floating-point 'type'.");
			}

			//Read the text data.
			std::stringstream strStream;
			for(const TiXmlNode *pNode = pAttribElem->FirstChild();
				pNode;
				pNode = pNode->NextSibling())
			{
				const TiXmlText *pText = pNode->ToText();
				if(pText)
				{
					strStream << pText->ValueStr() << " ";
				}
			}

			strStream.flush();
			const std::string &strTest = strStream.str();

			//Parse the text stream.
			pAttribType->ParseFunc(dataArray, strStream);
			if(dataArray.empty())
				throw std::runtime_error("The attribute's must have an array of values.");
			if(dataArray.size() % iSize != 0)
				throw std::runtime_error("The attribute's data must be a multiple of its size in elements.");
		}

		Attribute(const Attribute &rhs)
		{
			iAttribIx = rhs.iAttribIx;
			pAttribType = rhs.pAttribType;
			iSize = rhs.iSize;
			bIsIntegral = rhs.bIsIntegral;
			dataArray = rhs.dataArray;
		}

		Attribute &operator=(const Attribute &rhs)
		{
			iAttribIx = rhs.iAttribIx;
			pAttribType = rhs.pAttribType;
			iSize = rhs.iSize;
			bIsIntegral = rhs.bIsIntegral;
			dataArray = rhs.dataArray;
			return *this;
		}

		size_t NumElements() const
		{
			return dataArray.size() / iSize;
		}

		size_t CalcByteSize() const
		{
			return dataArray.size() * pAttribType->iNumBytes;
		}

		void FillBoundBufferObject(size_t iOffset) const
		{
			pAttribType->WriteToBuffer(GL_ARRAY_BUFFER, dataArray, iSize, iOffset);
		}

		void SetupAttributeArray(size_t iOffset) const
		{
			glEnableVertexAttribArray(iAttribIx);
			if(bIsIntegral)
			{
				glVertexAttribIPointer(iAttribIx, iSize, pAttribType->eGLType,
					0, (void*)iOffset);
			}
			else
			{
				glVertexAttribPointer(iAttribIx, iSize,
					pAttribType->eGLType, pAttribType->bNormalized ? GL_TRUE : GL_FALSE,
					0, (void*)iOffset);
			}
		}

		GLuint iAttribIx;
		const AttribType *pAttribType;
		int iSize;
		bool bIsIntegral;
		std::vector<AttribData> dataArray;
	};

	void ProcessVAO(const TiXmlElement *pVaoElem, std::string &strName, std::vector<GLuint> &attributes)
	{
		if(pVaoElem->QueryStringAttribute("name", &strName) != TIXML_SUCCESS)
			throw std::runtime_error("Missing 'name' attribute in a 'vao' element.");

		for(const TiXmlElement *pNode = pVaoElem->FirstChildElement();
			pNode;
			pNode = pNode->NextSiblingElement())
		{
			int iAttrib = -1;
			if(pNode->QueryIntAttribute("attrib", &iAttrib) != TIXML_SUCCESS)
				throw std::runtime_error("Missing 'attrib' attribute in a 'source' element.");

			attributes.push_back(iAttrib);
		}
	}

	struct IndexData
	{
		IndexData(const TiXmlElement *pIndexElem)
		{
			std::string strType;
			if(pIndexElem->QueryStringAttribute("type", &strType) != TIXML_SUCCESS)
				throw std::runtime_error("Missing 'type' attribute in an 'index' element.");

			if(strType != "uint" && strType != "ushort" && strType != "ubyte")
				throw std::runtime_error("Improper 'type' attribute value on 'index' element.");

			pAttribType = GetAttribType(strType);

			//Read the text data.
			std::stringstream strStream;
			for(const TiXmlNode *pNode = pIndexElem->FirstChild();
				pNode;
				pNode = pNode->NextSibling())
			{
				const TiXmlText *pText = pNode->ToText();
				if(pText)
				{
					strStream << pText->ValueStr() << " ";
				}
			}

			strStream.flush();
			const std::string &strTest = strStream.str();

			//Parse the text stream.
			pAttribType->ParseFunc(dataArray, strStream);
			if(dataArray.empty())
				throw std::runtime_error("The index element must have an array of values.");
		}

		IndexData()
			: pAttribType(NULL)
		{}

		IndexData(const IndexData &rhs)
		{
			pAttribType = rhs.pAttribType;
			dataArray = rhs.dataArray;
		}

		IndexData &operator=(const IndexData &rhs)
		{
			pAttribType = rhs.pAttribType;
			dataArray = rhs.dataArray;
			return *this;
		}

		size_t CalcByteSize() const
		{
			return dataArray.size() * pAttribType->iNumBytes;
		}

		void FillBoundBufferObject(size_t iOffset) const
		{
			pAttribType->WriteToBuffer(GL_ELEMENT_ARRAY_BUFFER, dataArray, 1, iOffset);
		}

		const AttribType *pAttribType;
		std::vector<AttribData> dataArray;
	};

	RenderCmd ProcessRenderCmd(const TiXmlElement *pCmdElem)
	{
		RenderCmd cmd;

		std::string strCmdName;
		if(pCmdElem->QueryStringAttribute("cmd", &strCmdName) != TIXML_SUCCESS)
			throw std::runtime_error("Missing 'cmd' attribute in an 'arrays' or 'indices' element.");

		int iArrayCount = ARRAY_COUNT(g_allPrimitiveTypes);
		const PrimitiveType *pPrim = std::find_if(
			g_allPrimitiveTypes, &g_allPrimitiveTypes[iArrayCount],
			std::bind1st(PrimitiveTypeFinder(), strCmdName));

		if(pPrim == &g_allPrimitiveTypes[iArrayCount])
			throw std::runtime_error("Unknown 'cmd' field.");

		cmd.ePrimType = pPrim->eGLPrimType;

		if(pCmdElem->ValueStr() == "indices")
		{
			cmd.bIsIndexedCmd = true;
			int iPrimRestart;
			if(pCmdElem->QueryIntAttribute("prim-restart", &iPrimRestart) == TIXML_SUCCESS)
			{
				if(iPrimRestart < 0)
					throw std::runtime_error("Attribute 'start' must be between 0 or greater.");
			}
			else
				iPrimRestart = -1;
			cmd.primRestart = iPrimRestart;
		}
		else if(pCmdElem->ValueStr() == "arrays")
		{
			cmd.bIsIndexedCmd = false;
			int iStart;
			if(pCmdElem->QueryIntAttribute("start", &iStart) != TIXML_SUCCESS)
				throw std::runtime_error("Missing 'start' attribute in an 'arrays' element.");
			if(iStart < 0)
				throw std::runtime_error("Attribute 'start' must be between 0 or greater.");
			cmd.start = iStart;

			int iCount;
			if(pCmdElem->QueryIntAttribute("count", &iCount) != TIXML_SUCCESS)
				throw std::runtime_error("Missing 'count' attribute in an 'arrays' element.");
			if(iCount <= 0)
				throw std::runtime_error("Attribute 'count' must be between 0 or greater.");
			cmd.elemCount = iCount;
		}
		else
			throw std::runtime_error("Bad element. Must be 'indices' or 'arrays'.");

		return cmd;
	}

	typedef std::map<std::string, GLuint> VAOMap;
	typedef VAOMap::value_type VAOMapData;

	struct MeshData
	{
		MeshData()
			: oAttribArraysBuffer(0)
			, oIndexBuffer(0)
			, oVAO(0)
		{}

		GLuint oAttribArraysBuffer;
		GLuint oIndexBuffer;
		GLuint oVAO;

		VAOMap namedVAOs;

		std::vector<RenderCmd> primatives;
	};

	Mesh::Mesh( const std::string &strFilename )
		: m_pData(new MeshData)
	{
		std::vector<Attribute> attribs;
		attribs.reserve(16);
		std::map<GLuint, int> attribIndexMap;	//Maps from attribute indices to 'attribs' indices.

		std::vector<IndexData> indexData;

		std::vector<std::pair<std::string, std::vector<GLuint> > > namedVaoList;

		{
			std::string strDataFilename = FindFileOrThrow(strFilename);
			std::ifstream fileStream(strDataFilename.c_str());
			if(!fileStream.is_open())
				throw std::runtime_error("Could not find the mesh file.");

			TiXmlDocument theDoc;

			fileStream >> theDoc;
			fileStream.close();

			if(theDoc.Error())
				throw std::runtime_error(theDoc.ErrorDesc());

			TiXmlHandle docHandle(&theDoc);

			const TiXmlElement *pProcNode = docHandle.FirstChild("mesh").FirstChild().ToElement();
			while(pProcNode->ValueStr() == "attribute")
			{
				attribs.push_back(Attribute(pProcNode));
				attribIndexMap[attribs.back().iAttribIx] = attribs.size() - 1;
				pProcNode = pProcNode->NextSiblingElement();
			}

			while(pProcNode->ValueStr() == "vao")
			{
				namedVaoList.push_back(std::pair<std::string, std::vector<GLuint> >());
				std::pair<std::string, std::vector<GLuint> > &namedVao = namedVaoList.back();
				ProcessVAO(pProcNode, namedVao.first, namedVao.second);
				pProcNode = pProcNode->NextSiblingElement();
			}

			for(; pProcNode; pProcNode = pProcNode->NextSiblingElement())
			{
				m_pData->primatives.push_back(ProcessRenderCmd(pProcNode));
				if(pProcNode->ValueStr() == "indices")
					indexData.push_back(IndexData(pProcNode));
			}
		}

		//Figure out how big of a buffer object for the attribute data we need.
		size_t iAttrbBufferSize = 0;
		std::vector<size_t> attribStartLocs;
		attribStartLocs.reserve(attribs.size());
		int iNumElements = 0;
		for(size_t iLoop = 0; iLoop < attribs.size(); iLoop++)
		{
			iAttrbBufferSize = iAttrbBufferSize % 16 ?
				(iAttrbBufferSize + (16 - iAttrbBufferSize % 16)) : iAttrbBufferSize;

			attribStartLocs.push_back(iAttrbBufferSize);
			const Attribute &attrib = attribs[iLoop];

			iAttrbBufferSize += attrib.CalcByteSize();

			if(iNumElements)
			{
				if(iNumElements != attrib.NumElements())
					throw std::runtime_error("Some of the attribute arrays have different element counts.");
			}
			else
				iNumElements = attrib.NumElements();
		}

		//Create the "Everything" VAO.
		glGenVertexArrays(1, &m_pData->oVAO);
		glBindVertexArray(m_pData->oVAO);

		//Create the buffer object.
		glGenBuffers(1, &m_pData->oAttribArraysBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, m_pData->oAttribArraysBuffer);
		glBufferData(GL_ARRAY_BUFFER, iAttrbBufferSize, NULL, GL_STATIC_DRAW);

		//Fill in our data and set up the attribute arrays.
		for(size_t iLoop = 0; iLoop < attribs.size(); iLoop++)
		{
			const Attribute &attrib = attribs[iLoop];
			attrib.FillBoundBufferObject(attribStartLocs[iLoop]);
			attrib.SetupAttributeArray(attribStartLocs[iLoop]);
		}

		//Fill the named VAOs.
		for(size_t iLoop = 0; iLoop < namedVaoList.size(); iLoop++)
		{
			std::pair<std::string, std::vector<GLuint> > &namedVao = namedVaoList[iLoop];
			GLuint vao = -1;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			for(size_t iAttribIx = 0; iAttribIx < namedVao.second.size(); iAttribIx++)
			{
				GLuint iAttrib = namedVao.second[iAttribIx];
				int iAttribOffset = -1;
				for(size_t iCount = 0; iCount < attribs.size(); iCount++)
				{
					if(attribs[iCount].iAttribIx == iAttrib)
					{
						iAttribOffset = iCount;
						break;
					}
				}

				const Attribute &attrib = attribs[iAttribOffset];
				attrib.SetupAttributeArray(attribStartLocs[iAttribOffset]);
			}

			m_pData->namedVAOs[namedVao.first] = vao;
		}

		glBindVertexArray(0);

		//Get the size of our index buffer data.
		size_t iIndexBufferSize = 0;
		std::vector<size_t> indexStartLocs;
		indexStartLocs.reserve(indexData.size());
		for(size_t iLoop = 0; iLoop < indexData.size(); iLoop++)
		{
			iIndexBufferSize = iIndexBufferSize % 16 ?
				(iIndexBufferSize + (16 - iIndexBufferSize % 16)) : iIndexBufferSize;

			indexStartLocs.push_back(iIndexBufferSize);
			const IndexData &currData = indexData[iLoop];

			iIndexBufferSize += currData.CalcByteSize();
		}

		//Create the index buffer object.
		if(iIndexBufferSize)
		{
			glBindVertexArray(m_pData->oVAO);

			glGenBuffers(1, &m_pData->oIndexBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pData->oIndexBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, iIndexBufferSize, NULL, GL_STATIC_DRAW);

			//Fill with data.
			for(size_t iLoop = 0; iLoop < indexData.size(); iLoop++)
			{
				const IndexData &currData = indexData[iLoop];
				currData.FillBoundBufferObject(indexStartLocs[iLoop]);
			}

			//Fill in indexed rendering commands.
			size_t iCurrIndexed = 0;
			for(size_t iLoop = 0; iLoop < m_pData->primatives.size(); iLoop++)
			{
				RenderCmd &prim = m_pData->primatives[iLoop];
				if(prim.bIsIndexedCmd)
				{
					prim.start = (GLuint)indexStartLocs[iCurrIndexed];
					prim.elemCount = (GLuint)indexData[iCurrIndexed].dataArray.size();
					prim.eIndexDataType = indexData[iCurrIndexed].pAttribType->eGLType;
					iCurrIndexed++;
				}
			}

			VAOMap::iterator endIt = m_pData->namedVAOs.end();
			for(VAOMap::iterator currIt = m_pData->namedVAOs.begin();
				currIt != endIt;
				++currIt)
			{
				VAOMapData &data = *currIt;
				glBindVertexArray(data.second);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pData->oIndexBuffer);
			}

			glBindVertexArray(0);
		}
	}

	Mesh::~Mesh()
	{
		delete m_pData;
	}

	void Mesh::Render() const
	{
		if(!m_pData->oVAO)
			return;

		glBindVertexArray(m_pData->oVAO);
		std::for_each(m_pData->primatives.begin(), m_pData->primatives.end(),
			std::mem_fun_ref(&RenderCmd::Render));
		glBindVertexArray(0);
	}

	void Mesh::Render( const std::string &strMeshName ) const
	{
		VAOMap::const_iterator theIt = m_pData->namedVAOs.find(strMeshName);
		if(theIt == m_pData->namedVAOs.end())
			return;

		glBindVertexArray(theIt->second);
		std::for_each(m_pData->primatives.begin(), m_pData->primatives.end(),
			std::mem_fun_ref(&RenderCmd::Render));
		glBindVertexArray(0);
	}

	void Mesh::DeleteObjects()
	{
		glDeleteBuffers(1, &m_pData->oAttribArraysBuffer);
		m_pData->oAttribArraysBuffer = 0;
		glDeleteBuffers(1, &m_pData->oIndexBuffer);
		m_pData->oIndexBuffer = 0;
		glDeleteVertexArrays(1, &m_pData->oVAO);
		m_pData->oVAO = 0;

		VAOMap::iterator endIt = m_pData->namedVAOs.end();
		for(VAOMap::iterator currIt = m_pData->namedVAOs.begin();
			currIt != endIt;
			++currIt)
		{
			VAOMapData &data = *currIt;
			glDeleteVertexArrays(1, &data.second);
			data.second = 0;
		}
	}
}
