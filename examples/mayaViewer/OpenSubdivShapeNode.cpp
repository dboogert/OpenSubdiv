#ifdef __APPLE__
	#include <maya/OpenMayaMac.h>
#endif

#include <osd/mutex.h>
#include <osd/cpuDispatcher.h>

#include <maya/MTypes.h>

#include <maya/MFnPlugin.h>
#include <maya/MFnPluginData.h>
#include <maya/MGlobal.h>
#include <maya/MFnMesh.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MIntArray.h>
#include <maya/MUintArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MPointArray.h>
#include <maya/MItMeshEdge.h>

#include "hbrUtil.h"

class OpenSubdivShape : public MPxNode
{
	MStatus compute( const MPlug& plug, MDataBlock& dataBlock );
};

namespace
{
	MObject inputMeshAttribute;
	MObject outputMeshAttribute;

	void*	createProxy()
	{
		return new OpenSubdivShape();
	}

	MStatus  initializeAttributes()
	{
		MStatus status;

		MFnTypedAttribute typedAttribute;
		inputMeshAttribute = typedAttribute.create("inputMesh", "inputMesh", MFnData::kMesh, &status);
		MPxNode::addAttribute(inputMeshAttribute);

		outputMeshAttribute = typedAttribute.create("outputMesh", "outputMesh", MFnData::kMesh, &status);
		MPxNode::addAttribute(outputMeshAttribute);

		MPxNode::attributeAffects(inputMeshAttribute, outputMeshAttribute);
		return status;
	}

	MTypeId  getTypeId()
	{
		MTypeId id(123);
		return id;
	}
}

MStatus OpenSubdivShape::compute( const MPlug& plug, MDataBlock& dataBlock )
{
	MStatus status;
	if (plug == outputMeshAttribute)
	{
		MStatus s;

		MFnMesh meshFn(dataBlock.inputValue(inputMeshAttribute).asMesh());
		MIntArray vertexCount, vertexList;
		meshFn.getVertices(vertexCount, vertexList);
		MUintArray edgeIds;
		MDoubleArray edgeCreaseData;
		meshFn.getCreaseEdges(edgeIds, edgeCreaseData);
		MUintArray vtxIds;
		MDoubleArray vtxCreaseData;
		meshFn.getCreaseVertices(vtxIds, vtxCreaseData );


		std::vector<int> numIndices(vertexCount.length()), faceIndices(vertexList.length());
		std::vector<int> edgeCreaseIndices, vtxCreaseIndices(vtxIds.length());
		std::vector<float> edgeCreases(edgeCreaseData.length()), vtxCreases(vtxCreaseData.length());

		for(unsigned int i = 0; i < vertexCount.length(); ++i)
			numIndices[i] = vertexCount[i];
		for(unsigned int i = 0; i < vertexList.length(); ++i)
			faceIndices[i] = vertexList[i];
		for(unsigned int i = 0; i < vtxIds.length(); ++i)
			vtxCreaseIndices[i] = vtxIds[i];
		for(unsigned int i = 0; i < vtxCreaseData.length(); ++i)
			vtxCreases[i] = (float)vtxCreaseData[i];
		for(unsigned int i = 0; i < edgeCreaseData.length(); ++i)
			edgeCreases[i] = (float)edgeCreaseData[i];

		// edge crease index is stored as pair of <face id> <edge localid> ...
		int nEdgeIds = edgeIds.length();
		edgeCreaseIndices.resize(nEdgeIds*2);
		for(int i = 0; i < nEdgeIds; ++i){
			int2 vertices;
			if (meshFn.getEdgeVertices(edgeIds[i], vertices) != MS::kSuccess) {
				s.perror("ERROR can't get creased edge vertices");
				continue;
			}
			edgeCreaseIndices[i*2] = vertices[0];
			edgeCreaseIndices[i*2+1] = vertices[1];
		}

		//TODO read these from this nodes attributes
		bool loop = false;
		short interpBoundary = 0;
		short level = 1;

		OpenSubdiv::OsdHbrMesh *hbrMesh = ConvertToHBR(meshFn.numVertices(), numIndices, faceIndices,
													   vtxCreaseIndices, vtxCreases,
													   std::vector<int>(), std::vector<float>(),
													   edgeCreaseIndices, edgeCreases,
													   interpBoundary, loop);

		int kernel = OpenSubdiv::OsdKernelDispatcher::kCPU;
//		if (OpenSubdiv::OsdKernelDispatcher::HasKernelType(OpenSubdiv::OsdKernelDispatcher::kOPENMP)) {
//			kernel = OpenSubdiv::OsdKernelDispatcher::kOPENMP;
//		}

		OpenSubdiv::OsdMesh* osdMesh = new OpenSubdiv::OsdMesh();
		osdMesh->Create(hbrMesh, level, kernel);

		std::cerr << "compute" << std::endl;
//
//		OpenSubdiv::OsdVertexBuffer* posNormalBuffer =  osdMesh->InitializeVertexBuffer(3 * 2);
//
//		std::cerr << "Num Elements: " << posNormalBuffer->GetNumElements() << std::endl;

		delete hbrMesh;
		delete osdMesh;
//		delete posNormalBuffer;
	}

	return status;
}

MStatus initializePlugin( MObject obj )
{
    MStatus   status;
    MFnPlugin plugin( obj, PLUGIN_COMPANY, "3.0", "Any");

    status = plugin.registerNode("openSubdivShape", getTypeId(), &createProxy, &initializeAttributes, MPxNode::kDependNode);
    OpenSubdiv::OsdCpuKernelDispatcher::Register();

    return status;
}

MStatus uninitializePlugin( MObject obj)
{
    MStatus   status;
    MFnPlugin plugin( obj );

    return status;
}
