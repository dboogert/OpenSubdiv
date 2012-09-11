#ifdef __APPLE__
	#include <maya/OpenMayaMac.h>
#endif

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

class OpenSubdivShape : public MPxNode
{
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

MStatus initializePlugin( MObject obj )
{
    MStatus   status;
    MFnPlugin plugin( obj, PLUGIN_COMPANY, "3.0", "Any");

    status = plugin.registerNode("pixarOsd", getTypeId(), &createProxy, &initializeAttributes, MPxNode::kDependNode);

    return status;
}

MStatus uninitializePlugin( MObject obj)
{
    MStatus   status;
    MFnPlugin plugin( obj );

    return status;
}
