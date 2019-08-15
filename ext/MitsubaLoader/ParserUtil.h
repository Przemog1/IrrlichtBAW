#ifndef __I_PARSER_UTIL_H_INCLUDED__
#define __I_PARSER_UTIL_H_INCLUDED__

#include "../3rdparty/libexpat/expat/lib/expat.h"
#include "irr/asset/IAssetManager.h"

#include "../../ext/MitsubaLoader/CMitsubaScene.h"

#include "../../ext/MitsubaLoader/IElement.h"

#include "irr/asset/IAssetLoader.h"
#include "ISceneManager.h"
#include "IFileSystem.h"
#include "irr/asset/ICPUMesh.h"

namespace irr { namespace ext { namespace MitsubaLoader {

//now unsupported elements (like  <sensor> (for now), for example) and its children elements will be ignored
class ParserFlowController
{
public:
	ParserFlowController()
		:isParsingSuspendedFlag(false) {};

	bool suspendParsingIfElNotSupported(const char* _el);
	void checkForUnsuspend(const char* _el);

	inline bool isParsingSuspended() const { return isParsingSuspendedFlag; }

private:
	static constexpr const char* unsElements[] = { "integrator", "emitter", "ref", "bsdf", "sensor", "medium", nullptr };
	bool isParsingSuspendedFlag;
	std::string notSupportedElement;

};

class ParserLog
{
public:
	/*prints this message:
	Mitsuba loader error:
	Invalid .xml file structure: attribute 'attribName' is not declared for element 'elementName' */
	static void wrongAttribute(const std::string& attribName, const std::string& elementName);

	/*prints this message:
	Mitsuba loader error:
	Invalid .xml file structure: 'parentName' is not a parent of 'wrongChildName' */
	static void wrongChildElement(const std::string& parentName, const std::string& childName);

	/*prints this message:
	Mitsuba loader error:
	Invalid .xml file structure: message */
	static void mitsubaLoaderError(const std::string& errorMessage);

};


//struct, which will be passed to expat handlers as user data (first argument) see: XML_StartElementHandler or XML_EndElementHandler in expat.h
struct ParserData
{
	//! Constructor 
	ParserData(irr::asset::IAssetManager& _assetManager, XML_Parser _parser)
		: assetManager(_assetManager),
		scene(nullptr),
		parser(_parser)
	{
		
	}

	irr::asset::IAssetManager& assetManager;

	/*root element, which will hold all loaded assets and material data
	in irr::asset::SCPUMesh (for now) instance*/
	std::unique_ptr<CMitsubaScene> scene;

	/*array of currently processed elements
	each element of index N is parent of the element of index N+1
	the scene element is a parent of all elements of index 0 */
	core::vector<std::unique_ptr<IElement>> elements;

	XML_Parser parser;

	ParserFlowController pfc;
};

void elementHandlerStart(void* _data, const char* _el, const char** _atts);
void elementHandlerEnd(void* _data, const char* _el);

}
}
}

#endif