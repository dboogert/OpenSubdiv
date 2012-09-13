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
#include <maya/MFloatPointArray.h>
#include <maya/MFloatPoint.h>

#include <maya/MFnMeshData.h>
#include <maya/MFnMesh.h>

#include <osd/elementArrayBuffer.h>
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
		short level = 2;

		OpenSubdiv::OsdHbrMesh *hbrMesh = ConvertToHBR(meshFn.numVertices(), numIndices, faceIndices,
													   vtxCreaseIndices, vtxCreases,
													   std::vector<int>(), std::vector<float>(),
													   edgeCreaseIndices, edgeCreases,
													   interpBoundary, loop);

		int kernel = OpenSubdiv::OsdKernelDispatcher::kCPU;
		if (OpenSubdiv::OsdKernelDispatcher::HasKernelType(OpenSubdiv::OsdKernelDispatcher::kOPENMP)) {
			kernel = OpenSubdiv::OsdKernelDispatcher::kOPENMP;
		}

		OpenSubdiv::OsdMesh* osdMesh = new OpenSubdiv::OsdMesh();
		osdMesh->Create(hbrMesh, level, kernel);

		OpenSubdiv::OsdElementArrayBuffer* indexBuffer = osdMesh->CreateElementArrayBuffer(level);

		std::cerr << "compute" << std::endl;

		OpenSubdiv::OsdVertexBuffer* posNormalBuffer =  osdMesh->InitializeVertexBuffer(3);

		std::cerr << "Num Elements: " << posNormalBuffer->GetNumElements() << std::endl;

		int nPoints = meshFn.numVertices();
		const float *points = meshFn.getRawPoints(&s);

		MFloatVectorArray normals;
		meshFn.getVertexNormals(true, normals);

		// Update vertex
		std::vector<float> vertex;
		vertex.resize(nPoints*3);

		for(int i = 0; i < nPoints; ++i){
			vertex[i*3+0] = points[i*3+0];
			vertex[i*3+1] = points[i*3+1];
			vertex[i*3+2] = points[i*3+2];
//			vertex[i*6+3] = normals[i].x;
//			vertex[i*6+4] = normals[i].y;
//			vertex[i*6+5] = normals[i].z;
		}

		posNormalBuffer->UpdateData(&vertex.at(0), nPoints);

	/* XXX
		float *varying = new float[_osdmesh.GetNumVaryingElements()];
		_osdmesh.BeginUpdateCoarseVertexVarying();
		for(int i = 0; i < nPoints; ++i){
			varying[0] = normals[i].x;
			varying[1] = normals[i].y;
			varying[2] = normals[i].z;
			_osdmesh.UpdateCoarseVertexVarying(i, varying);
		}
		_osdmesh.EndUpdateCoarseVertexVarying();
		delete[] varying;
	*/

		// subdivide
		osdMesh->Subdivide(posNormalBuffer, NULL);

		OpenSubdiv::OsdCpuVertexBuffer* vertexBuffer = dynamic_cast<OpenSubdiv::OsdCpuVertexBuffer*>(posNormalBuffer);

		if(vertexBuffer)
		{
			std::cerr << "vbo size: " << vertexBuffer->GetSize() << std::endl;
		}
		std::cerr << "index buffer size: " << indexBuffer->GetNumIndices() << std::endl;

		// Create a new mesh using the index buffer & vertex buffer
		// The problem is with the interpolated UVs
		//

		// num (indices / 4) quad mesh in size
		// should be able to create a position array simply

		MFnMeshData dataCreator;
		MObject newOutputData = dataCreator.create(&s);

		int numVertices = vertexBuffer->GetSize() / vertexBuffer->GetNumElements();
		int numFaces = indexBuffer->GetNumIndices() / 4;

		MFloatPointArray positions;
		positions.setLength(numVertices);

		MIntArray faceCounts;
		faceCounts.setLength(numFaces);

		MIntArray faces;
		faces.setLength(numFaces * 4);

		const float* positionBuffer = vertexBuffer->GetCpuBuffer();

		for (int i = 0; i < numVertices; ++i)
			positions[i] = MFloatPoint(positionBuffer[i * 3 + 0], positionBuffer[i * 3 + 1],positionBuffer[i * 3 + 2]);

		for (int i = 0; i < numFaces; ++i)
			faceCounts[i] = 4;

		const std::vector<int>& indices = osdMesh->GetFarMesh()->GetFaceVertices(level);
		for (int i = 0; i < numFaces * 4; ++i)
			faces[i] = indices[i];

		MObject newMesh = meshFn.create(numVertices, numFaces,
		                                    positions, faceCounts, faces,
		                                    newOutputData, &s);

		MDataHandle outputHandle = dataBlock.outputValue(outputMeshAttribute, &s);
		outputHandle.set(newOutputData);

		delete hbrMesh;
		delete osdMesh;
		delete posNormalBuffer;
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
