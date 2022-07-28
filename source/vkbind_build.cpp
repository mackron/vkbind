#include "external/tinyxml2.cpp"
#include <string>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <assert.h>

#define VKB_BUILD_XML_PATH      "../../resources/vk.xml"
#define VKB_BUILD_TEMPLATE_PATH "../../source/vkbind_template.h"

typedef int vkbResult;
#define VKB_SUCCESS                 0
#define VKB_ERROR                   -1
#define VKB_INVALID_ARGS            -2
#define VKB_OUT_OF_MEMORY           -3
#define VKB_FILE_TOO_BIG            -4
#define VKB_FAILED_TO_OPEN_FILE     -5
#define VKB_FAILED_TO_READ_FILE     -6
#define VKB_FAILED_TO_WRITE_FILE    -7

std::string vkbLTrim(const std::string &s)
{
    std::string result = s;
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](int character) { return !std::isspace(character); }));
    return result;
}

std::string vkbRTrim(const std::string &s)
{
    std::string result = s;
    result.erase(std::find_if(result.rbegin(), result.rend(), [](int character) { return !std::isspace(character); }).base(), result.end());
    return result;
}

std::string vkbTrim(const std::string &s)
{
    return vkbLTrim(vkbRTrim(s));
}

std::string vkbReplaceAll(const std::string &source, const std::string &from, const std::string &to)
{
    std::string result;
    std::string::size_type lastPos = 0;

    for (;;) {
        std::string::size_type findPos = source.find(from, lastPos);
        if (findPos == std::string::npos) {
            break;
        }

        result.append(source, lastPos, findPos - lastPos);
        result.append(to);
        lastPos = findPos + from.length();
    }

    result.append(source.substr(lastPos));
    return result;
}

void vkbReplaceAllInline(std::string &source, const std::string &from, const std::string &to)
{
    source = vkbReplaceAll(source, from, to);
}




vkbResult vkbFOpen(const char* filePath, const char* openMode, FILE** ppFile)
{
    if (filePath == NULL || openMode == NULL || ppFile == NULL) {
        return VKB_INVALID_ARGS;
    }

#if defined(_MSC_VER) && _MSC_VER > 1400   /* 1400 = Visual Studio 2005 */
    {
        if (fopen_s(ppFile, filePath, openMode) != 0) {
            return VKB_FAILED_TO_OPEN_FILE;
        }
    }
#else
    {
        FILE* pFile = fopen(filePath, openMode);
        if (pFile == NULL) {
            return VKB_FAILED_TO_OPEN_FILE;
        }

        *ppFile = pFile;
    }
#endif

    return VKB_SUCCESS;
}

vkbResult vkbOpenAndReadFileWithExtraData(const char* filePath, size_t* pFileSizeOut, void** ppFileData, size_t extraBytes)
{
    vkbResult result;
    FILE* pFile;
    uint64_t fileSize;
    void* pFileData;
    size_t bytesRead;

    /* Safety. */
    if (pFileSizeOut) *pFileSizeOut = 0;
    if (ppFileData) *ppFileData = NULL;

    if (filePath == NULL) {
        return VKB_INVALID_ARGS;
    }

    result = vkbFOpen(filePath, "rb", &pFile);
    if (result != VKB_SUCCESS) {
        return VKB_FAILED_TO_OPEN_FILE;
    }

    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (fileSize + extraBytes > SIZE_MAX) {
        fclose(pFile);
        return VKB_FILE_TOO_BIG;
    }

    pFileData = malloc((size_t)fileSize + extraBytes);    /* <-- Safe cast due to the check above. */
    if (pFileData == NULL) {
        fclose(pFile);
        return VKB_OUT_OF_MEMORY;
    }

    bytesRead = fread(pFileData, 1, (size_t)fileSize, pFile);
    if (bytesRead != fileSize) {
        free(pFileData);
        fclose(pFile);
        return VKB_FAILED_TO_READ_FILE;
    }

    fclose(pFile);

    if (pFileSizeOut) {
        *pFileSizeOut = (size_t)fileSize;
    }

    if (ppFileData) {
        *ppFileData = pFileData;
    } else {
        free(pFileData);
    }

    return VKB_SUCCESS;
}

vkbResult vkbOpenAndReadFile(const char* filePath, size_t* pFileSizeOut, void** ppFileData)
{
    return vkbOpenAndReadFileWithExtraData(filePath, pFileSizeOut, ppFileData, 0);
}

vkbResult vkbOpenAndReadTextFile(const char* filePath, size_t* pFileSizeOut, char** ppFileData)
{
    size_t fileSize;
    char* pFileData;
    vkbResult result = vkbOpenAndReadFileWithExtraData(filePath, &fileSize, (void**)&pFileData, 1);
    if (result != VKB_SUCCESS) {
        return result;
    }

    pFileData[fileSize] = '\0';

    if (pFileSizeOut) {
        *pFileSizeOut = fileSize;
    }

    if (ppFileData) {
        *ppFileData = pFileData;
    } else {
        free(pFileData);
    }

    return VKB_SUCCESS;
}

vkbResult vkbOpenAndWriteFile(const char* filePath, const void* pData, size_t dataSize)
{
    vkbResult result;
    FILE* pFile;

    if (filePath == NULL) {
        return VKB_INVALID_ARGS;
    }

    result = vkbFOpen(filePath, "wb", &pFile);
    if (result != VKB_SUCCESS) {
        return VKB_FAILED_TO_OPEN_FILE;
    }

    if (pData != NULL && dataSize > 0) {
        if (fwrite(pData, 1, dataSize, pFile) != dataSize) {
            fclose(pFile);
            return VKB_FAILED_TO_WRITE_FILE;
        }
    }

    fclose(pFile);
    return VKB_SUCCESS;
}

vkbResult vkbOpenAndWriteTextFile(const char* filePath, const char* text)
{
    if (text == NULL) {
        text = "";
    }

    return vkbOpenAndWriteFile(filePath, text, strlen(text));
}





struct vkbBuildPlatform
{
    std::string name;       // The name of the platform ("win32", "ios", etc.)
    std::string protect;
};

struct vkbBuildTag
{
    std::string name;
    std::string author;
    std::string contact;
};

struct vkbBuildFunctionParameter
{
    std::string typeC;
    std::string type;
    std::string nameC;
    std::string name;
    std::string arrayEnum;
    std::string optional;
    std::string externsync;
};

struct vkbBuildFunctionPointer
{
    std::string name;
    std::string returnType;
    std::vector<vkbBuildFunctionParameter> params;
};

struct vkbBuildStructMember
{
    std::string typeC;
    std::string type;
    std::string nameC;
    std::string name;
    std::string arrayEnum;      // The name of the enum used for array sizes for array members.
    std::string comment;
    std::string values;         // Attribute
    std::string optional;       // Attribute
    std::string noautovalidity; // Attribute
    std::string len;            // Attribute
};

struct vkbBuildStruct
{
    std::vector<vkbBuildStructMember> members;
};

struct vkbBuildType
{
    std::string type;   // Set by the inner <type> tag.
    std::string name;
    std::string category;
    std::string alias;
    std::string requires;
    std::string bitvalues;
    std::string returnedonly;
    std::string parent;

    // Type-specific data.
    vkbBuildFunctionPointer funcpointer;
    vkbBuildStruct structData;
    std::string verbatimValue;    // A string containing verbatim C code to output.
};

struct vkbBuildEnum
{
    std::string name;
    std::string alias;
    std::string value;
    std::string bitpos;
};

struct vkbBuildEnums
{
    std::string name;
    std::string type;
    std::vector<vkbBuildEnum> enums;
};

struct vkbBuildCommand
{
    std::string returnTypeC;
    std::string returnType;
    std::string name;
    std::vector<vkbBuildFunctionParameter> parameters;
    std::string alias;
    std::string successcodes;
    std::string errorcodes;
};


struct vkbBuildRequireType
{
    std::string name;
};

struct vkbBuildRequireEnum
{
    std::string name;
    std::string alias;
    std::string value;      // If set, indicates a #define.
    std::string extends;
    std::string bitpos;
    std::string extnumber;
    std::string offset;
    std::string comment;
    std::string dir;        // Whether or not enum values should be negative. Will be set to "-" for VkResult because it uses negative values for enums.
};

struct vkbBuildRequireCommand
{
    std::string name;
};

struct vkbBuildRequire
{
    std::string feature;
    std::string extension;
    std::string comment;
    std::vector<vkbBuildRequireType> types;
    std::vector<vkbBuildRequireEnum> enums;
    std::vector<vkbBuildRequireCommand> commands;
};

struct vkbBuildFeature
{
    std::string api;
    std::string name;
    std::string number;
    std::string comment;
    std::vector<vkbBuildRequire> requires;
};

struct vkbBuildExtension
{
    std::string name;
    std::string number;
    std::string type;
    std::string requiresAttr;
    std::string platform;
    std::string author;
    std::string contact;
    std::string supported;  // If "disabled", no code generated.
    std::string promotedto;
    std::string deprecatedby;
    std::vector<vkbBuildRequire> requires;
};

struct vkbBuild
{
    std::vector<vkbBuildPlatform> platforms;
    std::vector<vkbBuildTag> tags;
    std::vector<vkbBuildType> types;
    std::vector<vkbBuildEnums> enums;
    std::vector<vkbBuildCommand> commands;
    std::vector<vkbBuildFeature> features;
    std::vector<vkbBuildExtension> extensions;
};


// Parses the <platforms> tag.
vkbResult vkbBuildParsePlatforms(vkbBuild &context, tinyxml2::XMLElement* pPlatformsElement)
{
    if (pPlatformsElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    for (tinyxml2::XMLNode* pChild = pPlatformsElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        assert(pChildElement != NULL);

        vkbBuildPlatform platform;
        platform.name = pChildElement->Attribute("name");
        platform.protect = pChildElement->Attribute("protect");

        // Vulkan is dropping support for Mir. Skip this one.
        if (platform.name == "mir") {
            continue;
        }

        context.platforms.push_back(platform);
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseTags(vkbBuild &context, tinyxml2::XMLElement* pTagsElement)
{
    if (pTagsElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    for (tinyxml2::XMLNode* pChild = pTagsElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        assert(pChildElement != NULL);

        vkbBuildTag tag;
        tag.name = pChildElement->Attribute("name");
        tag.author = pChildElement->Attribute("author");
        tag.contact = pChildElement->Attribute("contact");
        context.tags.push_back(tag);
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseTypeNamePair(std::string &typeC, std::string &type, std::string &nameC, std::string &name, std::string &arrayEnum, tinyxml2::XMLElement* pXMLElement)
{
    if (pXMLElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    typeC = "";
    type  = "";
    nameC = "";
    name  = "";
    arrayEnum = "";

    // vk.xml is a bit annoying with how it declares it's types for function return types, parameters and variables. Non-trivial
    // pointer type looks like this: "const <type>void</type>*". In this case it's split over three child elements. The way we
    // parse this is not 100% perfect, but works for our purposes. We just enumerate over each child element until we find the
    // <name> tag. The same applies for the name, which can look something like this: "<name>int32_t</name>[2]" (note the array
    // specifier.
    tinyxml2::XMLNode* pChild = pXMLElement->FirstChild();

    // <type>
    while (pChild != NULL) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        if (pChildElement != NULL) {
            if (strcmp(pChildElement->Name(), "name") == 0) {
                break;  // Found the "<name>" tag which we use as the terminator for the type.
            }
            if (strcmp(pChildElement->Name(), "type") == 0) {
                type = pChildElement->FirstChild()->Value();
            }
            typeC += pChildElement->FirstChild()->Value();
        } else {
            typeC += pChild->Value();
        }

        pChild = pChild->NextSibling();
    }

    // <name> and <enum>.
    while (pChild != NULL) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        if (pChildElement != NULL) {
            if (strcmp(pChildElement->Name(), "comment") == 0) {
                break;  // Found a "<comment>" tag which we use as a terminator for the name.
            }
            if (strcmp(pChildElement->Name(), "enum") == 0) {
                arrayEnum = pChildElement->FirstChild()->Value();
            }
            if (strcmp(pChildElement->Name(), "name") == 0) {
                name = pChildElement->FirstChild()->Value();
            }
            nameC += pChildElement->FirstChild()->Value();
        } else {
            nameC += pChild->Value();
        }

        pChild = pChild->NextSibling();
    }

    typeC = vkbTrim(typeC);
    type  = vkbTrim(type);
    nameC = vkbTrim(nameC);
    name  = vkbTrim(name);
    arrayEnum = vkbTrim(arrayEnum);

    return VKB_SUCCESS;
}

std::string vkbExtractFuncionPointerReturnType(const std::string &value)
{
    // value = "typedef <type> (VKAPI_PTR *"
    size_t beg = strlen("typedef ");
    size_t end = value.find("(VKAPI_PTR *");
    return vkbTrim(value.substr(beg, end-beg));
}

vkbResult vkbBuildParseStructMember(vkbBuildStructMember &member, tinyxml2::XMLElement* pMemberElement)
{
    // The type can possible be split over multiple nodes. We just append the values of each node until we find the name node.
    std::string memberTypeC;
    std::string memberType;
    std::string memberNameC;
    std::string memberName;
    std::string memberArrayEnum;
    vkbResult result = vkbBuildParseTypeNamePair(memberTypeC, memberType, memberNameC, memberName, memberArrayEnum, pMemberElement);
    if (result != VKB_SUCCESS) {
        return result;
    }

    for (tinyxml2::XMLNode* pChild = pMemberElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        if (pChildElement != NULL) {
            if (strcmp(pChildElement->Name(), "comment") == 0) {
                member.comment = vkbTrim(pChildElement->FirstChild()->Value());
            }
        }
    }

    member.typeC = vkbTrim(memberTypeC);
    member.type  = vkbTrim(memberType);
    member.nameC = vkbTrim(memberNameC);
    member.name  = vkbTrim(memberName);
    member.arrayEnum = vkbTrim(memberArrayEnum);

    const char* values = pMemberElement->Attribute("values");
    const char* optional = pMemberElement->Attribute("optional");
    const char* noautovalidity = pMemberElement->Attribute("noautovalidity");
    const char* len = pMemberElement->Attribute("len");

    member.values = (values != NULL) ? values : "";
    member.optional = (optional != NULL) ? optional : "";
    member.noautovalidity = (noautovalidity != NULL) ? noautovalidity : "";
    member.len = (len != NULL) ? len : "";

    return VKB_SUCCESS;
}

void vkbBuildParseFuncPointerParams(vkbBuild &context, const std::string &paramString, vkbBuildType &type)
{
    (void)context;

    // First of all we need to clean the string. It will start with ")(" and will end with ");".
    std::string paramStringClean = vkbReplaceAll(vkbReplaceAll(paramString, ")(", ""), ");", "");

    // Now we need to split the string for each parameter. Each parameter will be separated by a comma.
    std::vector<std::string> params;
    const char* paramBeg = paramStringClean.c_str();
    for (;;) {
        const char* paramEnd = strchr(paramBeg, ',');
        if (paramEnd != NULL) {
            // Not the last parameter.
            params.push_back(vkbTrim(std::string(paramBeg, paramEnd-paramBeg)));
            paramBeg = paramEnd + 1;    // <-- Skip past the ",".
            continue;
        } else {
            // Reached the last parameter. It could also be a situation where there is no parameters. To handle this we just need
            // to check if the trimmed parameter string has any length.
            std::string param = vkbTrim(paramBeg);
            if (param.length() > 0) {
                params.push_back(param);
            }
            break;
        }
    }

    // At this point we should have cleanly trimmed strings for each parameter. The separator between the type and the name is the last
    // space character.
    for (auto param : params) {
        // There could be a case where there are no parameters, but there will be a "void" in the parameter list. Just skip this.
        if (param == "void") {
            continue;
        }

        const char* paramStr = param.c_str();
        std::string paramType;
        std::string paramName;

        const char* lastSpace = strrchr(paramStr, ' ');
        paramType = vkbTrim(std::string(paramStr, lastSpace-paramStr));
        paramName = vkbTrim(std::string(lastSpace));

        // At this point we have the name, and most of the type. We just need to do one last bit of parsing for the type to extract the
        // <type> tag and construct the C type string. To do this, we need to find the <type> tag (if any - it should always exist, but
        // I'll build this with the assumption that it may not). The C string is easy - it's the whole paramType string with the <type>
        // and </type> segments removed. The Vulkan type (the part inside the <type> tag) is also quite easy - we just find both <type>
        // and </type> and take the contents.
        std::string paramTypeC = vkbReplaceAll(vkbReplaceAll(paramType, "<type>", ""), "</type>", "");

        std::string paramTypeVK;
        const char* tagOpening = strstr(paramStr, "<type>");
        const char* tagClosing = NULL;
        if (tagOpening != NULL) {
            tagClosing = strstr(paramStr, "</type>");
            if (tagClosing != NULL) {
                const char* typeNameBeg = tagOpening + strlen("<type>");
                const char* typeNameEnd = tagClosing;
                paramTypeVK = std::string(typeNameBeg, typeNameEnd-typeNameBeg);
            }
        }
        
        // And we're done!
        vkbBuildFunctionParameter newParam;
        newParam.name = paramName;
        newParam.nameC = paramName;
        newParam.type = paramTypeVK;
        newParam.typeC = paramTypeC;
        type.funcpointer.params.push_back(newParam);
    }
    

    return;
}

vkbResult vkbBuildParseTypes(vkbBuild &context, tinyxml2::XMLElement* pTagsElement)
{
    if (pTagsElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    for (tinyxml2::XMLNode* pChild = pTagsElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        assert(pChildElement != NULL);

        // Ignore <comment> tags.
        if (strcmp(pChildElement->Name(), "comment") == 0) {
            continue;
        }

        const char* name = pChildElement->Attribute("name");
        const char* category = pChildElement->Attribute("category");
        const char* alias = pChildElement->Attribute("alias");
        const char* requires = pChildElement->Attribute("requires");
        const char* bitvalues = pChildElement->Attribute("bitvalues");
        const char* returnedonly = pChildElement->Attribute("returnedonly");
        const char* parent = pChildElement->Attribute("parent");

        vkbBuildType type;
        type.name = (name != NULL) ? vkbTrim(name) : "";
        type.category = (category != NULL) ? vkbTrim(category) : "";
        type.alias = (alias != NULL) ? vkbTrim(alias) : "";
        type.requires = (requires != NULL) ? vkbTrim(requires) : "";
        type.bitvalues = (bitvalues != NULL) ? vkbTrim(bitvalues) : "";
        type.returnedonly = (returnedonly != NULL) ? returnedonly : "";
        type.parent = (parent != NULL) ? parent : "";

        if (strcmp(type.category.c_str(), "funcpointer") == 0) {
            tinyxml2::XMLNode* pFirstChild = pChildElement->FirstChild();
            if (pFirstChild != NULL) {
                // The first child will contain the "typedef <return type> (VKAPI_PTR *" part. We only care about the return type (the other parts
                // can be reconstructed). To reconstruct this we just find the end of "typedef " and the start of the "(VKAPI_PTR *" part and use
                // that as the return type.
                type.funcpointer.returnType = vkbExtractFuncionPointerReturnType(pFirstChild->Value());
                
                // The second child should be the name of the function with "PFN_" appended to it.
                tinyxml2::XMLNode* pSecondChild = pFirstChild->NextSibling();
                if (pSecondChild != NULL) {
                    type.funcpointer.name = pSecondChild->FirstChild()->Value();
                    type.name = type.funcpointer.name;

                    // This is the part that get's annoying. For some reason, funcpointer types are declared as just a normal C function declaration
                    // with <type> tags wrapped around the types of each parameter. The annoying part is that the <type> tags don't wrap around the
                    // _whole_ type declaration. Instead it looks something like this: "const <type>void</type>* pSomeParam". What happens here is
                    // that the XML parser interprets this as three nodes: "const ", "<type>void</type>" and "* pSomeParam". This makes parsing non-
                    // trivial.
                    //
                    // The way I'm working around this is by concatenating the parameters as one big string, then having a custom function to parse
                    // this string manually, outside of tinyxml.
                    std::string paramString;
                    for (tinyxml2::XMLNode* pParamNode = pSecondChild->NextSibling(); pParamNode != NULL; pParamNode = pParamNode->NextSibling()) {
                        if (pParamNode->FirstChild() == NULL) {
                            paramString = paramString + pParamNode->Value();
                        } else {
                            paramString = paramString + "<" + pParamNode->Value() + ">" + pParamNode->FirstChild()->Value() + "</" + pParamNode->Value() + ">";
                        }
                    }

                    vkbBuildParseFuncPointerParams(context, paramString, type);
                }
            }
        }

        if (strcmp(type.category.c_str(), "struct") == 0 || strcmp(type.category.c_str(), "union") == 0) {
            for (tinyxml2::XMLNode* pMemberNode = pChildElement->FirstChild(); pMemberNode != NULL; pMemberNode = pMemberNode->NextSibling()) {
                tinyxml2::XMLElement* pMemberElement = pMemberNode->ToElement();
                if (pMemberElement != NULL) {
                    if (strcmp(pMemberElement->Name(), "comment") == 0) {
                        continue;   // Ignore <comment> tags.
                    }

                    vkbBuildStructMember member;
                    vkbBuildParseStructMember(member, pMemberElement);
                    type.structData.members.push_back(member);
                }
            }
        }

        if (strcmp(type.category.c_str(), "define") == 0 || strcmp(type.category.c_str(), "basetype") == 0) {
            for (tinyxml2::XMLNode* pMemberNode = pChildElement->FirstChild(); pMemberNode != NULL; pMemberNode = pMemberNode->NextSibling()) {
                if (pMemberNode->FirstChild() != NULL) {
                    if (strcmp(pMemberNode->Value(), "name") == 0) {
                        type.name = pMemberNode->FirstChild()->Value();
                    }
                    if (strcmp(pMemberNode->Value(), "type") == 0) {
                        type.type = pMemberNode->FirstChild()->Value();
                    }

                    // Always make sure there's a space between the previous content and the new content.
                    if (type.verbatimValue.length() > 0 && type.verbatimValue[type.verbatimValue.length()-1] != ' ') {
                        type.verbatimValue += " ";
                    }
                    type.verbatimValue += pMemberNode->FirstChild()->Value();
                } else {
                    type.verbatimValue += pMemberNode->Value();
                }
            }
        }

        if (strcmp(type.category.c_str(), "bitmask") == 0 || strcmp(type.category.c_str(), "handle") == 0) {
            for (tinyxml2::XMLNode* pMemberNode = pChildElement->FirstChild(); pMemberNode != NULL; pMemberNode = pMemberNode->NextSibling()) {
                if (pMemberNode->FirstChild() != NULL) {
                    if (strcmp(pMemberNode->Value(), "type") == 0) {
                        type.type = pMemberNode->FirstChild()->Value();
                    }
                    if (strcmp(pMemberNode->Value(), "name") == 0) {
                        type.name = pMemberNode->FirstChild()->Value();
                    }
                }
            }
        }
        
        context.types.push_back(type);
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseEnums(vkbBuild &context, tinyxml2::XMLElement* pEnumsElement)
{
    if (pEnumsElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* name = pEnumsElement->Attribute("name");
    const char* type = pEnumsElement->Attribute("type");

    vkbBuildEnums enums;
    enums.name = (name != NULL) ? vkbTrim(name) : "";
    enums.type = (type != NULL) ? vkbTrim(type) : "";

    for (tinyxml2::XMLNode* pChild = pEnumsElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        if (pChildElement == NULL) {
            continue;   /* Skip over non-element items such as comments. */
        }

        assert(pChildElement != NULL);

        if (strcmp(pChildElement->Name(), "enum") == 0) {
            const char* childName = pChildElement->Attribute("name");
            const char* childAlias = pChildElement->Attribute("alias");
            const char* childValue = pChildElement->Attribute("value");
            const char* childBitPos = pChildElement->Attribute("bitpos");

            vkbBuildEnum theEnum;
            theEnum.name = (childName != NULL) ? vkbTrim(childName) : "";
            theEnum.alias = (childAlias != NULL) ? vkbTrim(childAlias) : "";
            theEnum.value = (childValue != NULL) ? vkbTrim(childValue) : "";
            theEnum.bitpos = (childBitPos != NULL) ? vkbTrim(childBitPos) : "";

            // There's an <enums> tag that's specifically used for "#define" style enums. These are treated slightly differently. In this case, the <enums> type
            // will be empty, but we store a separate vkbBuildEnums object for each item within that <enums> tag. This object will contain only a single item,
            // which is the name/value of the #define.
            if (enums.type == "") {
                // #define enum.
                vkbBuildEnums defineEnum;
                defineEnum.name = childName;
                defineEnum.type = ""; // Should always be "".
                defineEnum.enums.push_back(theEnum);
                context.enums.push_back(defineEnum);
            } else {
                // Regular enum.
                enums.enums.push_back(theEnum);
            }
        }
    }

    if (enums.type != "") {
        context.enums.push_back(enums);
    }
    
    return VKB_SUCCESS;
}



vkbResult vkbBuildParseCommandProto(vkbBuildCommand &command, tinyxml2::XMLElement* pProtoElement)
{
    if (pProtoElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    std::string nameCUnused;
    std::string arrayEnumUnused;
    vkbResult result = vkbBuildParseTypeNamePair(command.returnTypeC, command.returnType, nameCUnused, command.name, arrayEnumUnused, pProtoElement);
    if (result != VKB_SUCCESS) {
        return result;
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseCommandParam(vkbBuildFunctionParameter &param, tinyxml2::XMLElement* pParamElement)
{
    if (pParamElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    vkbResult result = vkbBuildParseTypeNamePair(param.typeC, param.type, param.nameC, param.name, param.arrayEnum, pParamElement);
    if (result != VKB_SUCCESS) {
        return result;
    }

    const char* optional = pParamElement->Attribute("optional");
    const char* externsync = pParamElement->Attribute("externsync");

    param.optional = (optional != NULL) ? optional : "";
    param.externsync = (externsync != NULL) ? externsync : "";

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseCommand(vkbBuildCommand &command, tinyxml2::XMLElement* pCommandElement)
{
    if (pCommandElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* successcodes = pCommandElement->Attribute("successcodes");
    const char* errorcodes = pCommandElement->Attribute("errorcodes");

    command.successcodes = (successcodes != NULL) ? successcodes : "";
    command.errorcodes = (errorcodes != NULL) ? errorcodes : "";

    for (tinyxml2::XMLNode* pChild = pCommandElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        assert(pChildElement != NULL);

        if (strcmp(pChildElement->Name(), "proto") == 0) {
            vkbBuildParseCommandProto(command, pChildElement);
        }

        if (strcmp(pChildElement->Name(), "param") == 0) {
            vkbBuildFunctionParameter param;
            vkbBuildParseCommandParam(param, pChildElement);
            command.parameters.push_back(param);
        }
    }

    // There's a possibility that a command is actually just an alias for an existing item. In this case, the name
    // will be specified in the "name" attribute, and there will be an "alias" attribute with it.
    const char* nameAttr = pCommandElement->Attribute("name");
    if (nameAttr != NULL) {
        command.name = nameAttr;
    }

    const char* aliasAttr = pCommandElement->Attribute("alias");
    if (aliasAttr != NULL) {
        command.alias = aliasAttr;
    }
    
    return VKB_SUCCESS;
}

vkbResult vkbBuildParseCommands(vkbBuild &context, tinyxml2::XMLElement* pCommandsElement)
{
    if (pCommandsElement == NULL) {
        return VKB_INVALID_ARGS;
    }
    
    for (tinyxml2::XMLNode* pChild = pCommandsElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        assert(pChildElement != NULL);

        if (strcmp(pChildElement->Name(), "command") == 0) {
            vkbBuildCommand command;
            vkbResult result = vkbBuildParseCommand(command, pChildElement);
            if (result == VKB_SUCCESS) {
                context.commands.push_back(command);
            }
        }
    }

    return VKB_SUCCESS;
}


vkbResult vkbBuildParseRequireType(vkbBuildRequireType &type, tinyxml2::XMLElement* pRequireTypeElement)
{
    if (pRequireTypeElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* name = pRequireTypeElement->Attribute("name");
    if (name != NULL) {
        type.name = vkbTrim(name);
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseRequireEnum(vkbBuildRequireEnum &theEnum, tinyxml2::XMLElement* pRequireEnumElement)
{
    if (pRequireEnumElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* name = pRequireEnumElement->Attribute("name");
    const char* alias = pRequireEnumElement->Attribute("alias");
    const char* value = pRequireEnumElement->Attribute("value");
    const char* extends = pRequireEnumElement->Attribute("extends");
    const char* bitpos = pRequireEnumElement->Attribute("bitpos");
    const char* extnumber = pRequireEnumElement->Attribute("extnumber");
    const char* offset = pRequireEnumElement->Attribute("offset");
    const char* comment = pRequireEnumElement->Attribute("comment");
    const char* dir = pRequireEnumElement->Attribute("dir");

    theEnum.name = (name != NULL) ? vkbTrim(name) : "";
    theEnum.alias = (alias != NULL) ? vkbTrim(alias) : "";
    theEnum.value = (value != NULL) ? vkbTrim(value) : "";
    theEnum.extends = (extends != NULL) ? vkbTrim(extends) : "";
    theEnum.bitpos = (bitpos != NULL) ? vkbTrim(bitpos) : "";
    theEnum.extnumber = (extnumber != NULL) ? vkbTrim(extnumber) : "";
    theEnum.offset = (offset != NULL) ? vkbTrim(offset) : "";
    theEnum.comment = (comment != NULL) ? vkbTrim(comment) : "";
    theEnum.dir = (dir != NULL) ? vkbTrim(dir) : "";

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseRequireCommand(vkbBuildRequireCommand &command, tinyxml2::XMLElement* pRequireCommandElement)
{
    if (pRequireCommandElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* name = pRequireCommandElement->Attribute("name");
    if (name != NULL) {
        command.name = vkbTrim(name);
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseRequire(vkbBuildRequire &require, tinyxml2::XMLElement* pRequireElement)
{
    if (pRequireElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* feature = pRequireElement->Attribute("feature");
    const char* extension = pRequireElement->Attribute("extension");
    const char* comment = pRequireElement->Attribute("comment");

    require.feature = (feature != NULL) ? vkbTrim(feature) : "";
    require.extension = (extension != NULL) ? vkbTrim(extension) : "";
    require.comment = (comment != NULL) ? comment : "";

    for (tinyxml2::XMLNode* pChild = pRequireElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        if (pChildElement == NULL) {
            continue;
        }

        if (strcmp(pChildElement->Name(), "type") == 0) {
            vkbBuildRequireType type;
            vkbBuildParseRequireType(type, pChildElement);
            require.types.push_back(type);
        }
        if (strcmp(pChildElement->Name(), "enum") == 0) {
            vkbBuildRequireEnum theEnum;
            vkbBuildParseRequireEnum(theEnum, pChildElement);
            require.enums.push_back(theEnum);
        }
        if (strcmp(pChildElement->Name(), "command") == 0) {
            vkbBuildRequireCommand command;
            vkbBuildParseRequireCommand(command, pChildElement);
            require.commands.push_back(command);
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseFeature(vkbBuild &context, tinyxml2::XMLElement* pFeatureElement)
{
    if (pFeatureElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* api = pFeatureElement->Attribute("api");
    const char* name = pFeatureElement->Attribute("name");
    const char* number = pFeatureElement->Attribute("number");
    const char* comment = pFeatureElement->Attribute("comment");

    vkbBuildFeature feature;
    feature.api = (api != NULL) ? vkbTrim(api) : "";
    feature.name = (name != NULL) ? vkbTrim(name) : "";
    feature.number = (number != NULL) ? vkbTrim(number) : "";
    feature.comment = (comment != NULL) ? comment : "";

    for (tinyxml2::XMLNode* pChild = pFeatureElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        if (pChildElement == NULL) {
            continue;   /* May hit this if the element is a comment. */
        }

        assert(pChildElement != NULL);

        if (strcmp(pChildElement->Name(), "require") == 0) {
            vkbBuildRequire require;
            vkbBuildParseRequire(require, pChildElement);
            feature.requires.push_back(require);
        }
    }

    context.features.push_back(feature);
    return VKB_SUCCESS;
}

vkbResult vkbBuildParseExtension(vkbBuild &context, tinyxml2::XMLElement* pExtensionElement)
{
    if (pExtensionElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    const char* name = pExtensionElement->Attribute("name");
    const char* number = pExtensionElement->Attribute("number");
    const char* type = pExtensionElement->Attribute("type");
    const char* requiresAttr = pExtensionElement->Attribute("requires");
    const char* platform = pExtensionElement->Attribute("platform");
    const char* author = pExtensionElement->Attribute("author");
    const char* contact = pExtensionElement->Attribute("contact");
    const char* supported = pExtensionElement->Attribute("supported");
    const char* promotedto = pExtensionElement->Attribute("promotedto");
    const char* deprecatedby = pExtensionElement->Attribute("deprecatedby");

    // We ignore "disabled" extensions.
    if (strcmp(supported, "disabled") == 0) {
        return VKB_INVALID_ARGS;    // Should I return VKB_SUCCESS here?
    }

    // Support for Mir is being dropped. Skip this.
    if (platform != NULL && strcmp(platform, "mir") == 0) {
        return VKB_INVALID_ARGS;
    }
    
    vkbBuildExtension extension;
    extension.name = (name != NULL) ? vkbTrim(name) : "";
    extension.number = (number != NULL) ? vkbTrim(number) : "";
    extension.type = (type != NULL) ? vkbTrim(type) : "";
    extension.requiresAttr = (requiresAttr != NULL) ? vkbTrim(requiresAttr) : "";
    extension.platform = (platform != NULL) ? vkbTrim(platform) : "";
    extension.author = (author != NULL) ? vkbTrim(author) : "";
    extension.contact = (contact != NULL) ? vkbTrim(contact) : "";
    extension.supported = (supported != NULL) ? vkbTrim(supported) : "";
    extension.promotedto = (promotedto != NULL) ? vkbTrim(promotedto) : "";
    extension.deprecatedby = (deprecatedby != NULL) ? vkbTrim(deprecatedby) : "";

    for (tinyxml2::XMLNode* pChild = pExtensionElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        assert(pChildElement != NULL);

        if (strcmp(pChildElement->Name(), "require") == 0) {
            vkbBuildRequire require;
            vkbBuildParseRequire(require, pChildElement);
            extension.requires.push_back(require);
        }
    }

    context.extensions.push_back(extension);

    // At this point the extension is sitting at the end, but we need to check if any of the already-added extensions are deprecated by this one. If so,
    // we need to move it to the end so it's sitting after it. The reason we need to do this is to ensure any aliases are output beforehand so that
    // typedef's and whatnot work as expected. A better way to do this is to analyze the list of extensions and re-arrange them based on the dependant types.
    for (size_t i = 0; i < context.extensions.size(); i += 1) {
        if (context.extensions[i].deprecatedby == extension.name) {
            context.extensions.push_back(context.extensions[i]);
            context.extensions.erase(context.extensions.begin() + i);
            break;
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildParseExtensions(vkbBuild &context, tinyxml2::XMLElement* pExtensionsElement)
{
    if (pExtensionsElement == NULL) {
        return VKB_INVALID_ARGS;
    }

    for (tinyxml2::XMLNode* pChild = pExtensionsElement->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        if (pChildElement != NULL) {
            if (strcmp(pChildElement->Name(), "extension") == 0) {
                vkbBuildParseExtension(context, pChildElement);
            }
        }
    }

    return VKB_SUCCESS;
}


bool vkbBuildFindTypeByName(vkbBuild &context, const char* name, size_t* pIndexOut)
{
    if (name == NULL) {
        return false;
    }

    for (size_t iType = 0; iType < context.types.size(); ++iType) {
        if (context.types[iType].name == name) {
            if (pIndexOut != NULL) {
                *pIndexOut = iType;
            }
            return true;
        }
    }

    return false;
}

bool vkbBuildFindEnumByName(vkbBuild &context, const char* name, size_t* pIndexOut)
{
    if (name == NULL) {
        return false;
    }

    for (size_t iEnum = 0; iEnum < context.enums.size(); ++iEnum) {
        if (context.enums[iEnum].name == name) {
            if (pIndexOut != NULL) {
                *pIndexOut = iEnum;
            }
            return true;
        }
    }

    return false;
}

bool vkbBuildFindEnumValue(vkbBuild &context, const std::string &name, vkbBuildEnum &value)
{
    /* Search through every enum. */
    for (size_t iEnum = 0; iEnum < context.enums.size(); ++iEnum) {
        /* Check each item in the enum. */
        for (auto item : context.enums[iEnum].enums) {
            if (item.name == name) {
                /* Found the item. If it's aliased, search recursively. Otherwise return the value. */
                if (item.alias == "") {
                    value = item;
                    return true;
                } else {
                    return vkbBuildFindEnumValue(context, item.alias, value);
                }
            }
        }
    }

    /* Getting here means we couldn't find the enum from the base list. We'll need to check features and extensions. */

    /* Features. */
    for (auto feature : context.features) {
        for (auto requires : feature.requires) {
            for (auto enumValue : requires.enums) {
                if (enumValue.name == name) {
                    if (enumValue.alias == "") {
                        value.name   = enumValue.name;
                        value.alias  = "";
                        value.value  = enumValue.value;
                        value.bitpos = enumValue.bitpos;
                        return true;
                    } else {
                        return vkbBuildFindEnumValue(context, enumValue.alias, value);
                    }
                }
            }
        }
    }

    /* Extensions. */
    for (auto extension : context.extensions) {
        for (auto requires : extension.requires) {
            for (auto enumValue : requires.enums) {
                if (enumValue.name == name) {
                    if (enumValue.alias == "") {
                        value.name   = enumValue.name;
                        value.alias  = "";
                        value.value  = enumValue.value;
                        value.bitpos = enumValue.bitpos;
                        return true;
                    } else {
                        return vkbBuildFindEnumValue(context, enumValue.alias, value);
                    }
                }
            }
        }
    }

    /* Getting here means we couldn't find anything. */
    return false;
}

bool vkbBuildFindCommandByName(vkbBuild &context, const char* name, size_t* pIndexOut)
{
    if (name == NULL) {
        return false;
    }

    for (size_t iCommand = 0; iCommand < context.commands.size(); ++iCommand) {
        if (context.commands[iCommand].name == name) {
            if (pIndexOut != NULL) {
                *pIndexOut = iCommand;
            }
            return true;
        }
    }

    return false;
}

bool vkbBuildFindExtensionByName(vkbBuild &context, const char* name, size_t* pIndexOut)
{
    if (name == NULL) {
        return false;
    }

    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        if (context.extensions[iExtension].name == name) {
            if (pIndexOut != NULL) {
                *pIndexOut = iExtension;
            }
            return true;
        }
    }

    return false;
}

std::string vkbBuildCleanDefineValue(const std::string &value)
{
    std::string result = vkbTrim(value);
    vkbReplaceAllInline(result, "\\\n", "");

    // Now we need to remove all line comments. If the line ends with a line comment we leave the new line
    // character in place, otherwise we remove the entire line.
    for (;;) {
        std::string::size_type findPos = result.find("//", 0);
        if (findPos == std::string::npos) {
            break;
        }

        // If the comment starts on a new line we want to remove the whole thing.
        bool deleteEOLCharacter = false;
        if (findPos == 0 || result[findPos-1] == '\n') {
            deleteEOLCharacter = true;
        }

        // Starting from findPos, we now need to search for the new line character.
        std::string::size_type eolPos = result.find("\n", findPos+2);
        if (eolPos != std::string::npos) {
            if (deleteEOLCharacter) {
                eolPos += 1;
            } else {
                if (result[eolPos-1] == '\r') {
                    eolPos -= 1;
                }
            }
        }

        result.erase(findPos, eolPos - findPos);
    }

    return result;
}

template <typename T>
bool vkbContains(const std::vector<T> &list, T value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}


bool vkbPriorFeatureContainsType(std::vector<std::vector<size_t>> &typesPerFeature, size_t thisFeatureIndex, size_t typeIndex)
{
    for (size_t iFeature = 0; iFeature < thisFeatureIndex; ++iFeature) {
        if (vkbContains(typesPerFeature[iFeature], typeIndex)) {
            return true;
        }
    }

    return false;
}

bool vkbPriorFeatureContainsEnum(std::vector<std::vector<size_t>> &enumsPerFeature, size_t thisFeatureIndex, size_t enumIndex)
{
    for (size_t iFeature = 0; iFeature < thisFeatureIndex; ++iFeature) {
        if (vkbContains(enumsPerFeature[iFeature], enumIndex)) {
            return true;
        }
    }

    return false;
}


vkbResult vkbBuildAddEnumDependencies(vkbBuild &context, const char* enumName, std::vector<size_t> &enumIndicesOut)
{
    if (enumName == NULL) {
        return VKB_INVALID_ARGS;
    }

    size_t enumIndex;
    if (!vkbBuildFindEnumByName(context, enumName, &enumIndex)) {
        return VKB_INVALID_ARGS;    // Couldn't find the base type
    }

    if (vkbContains(enumIndicesOut, enumIndex)) {
        return VKB_SUCCESS; // This enum has already been handled.
    }

    enumIndicesOut.push_back(enumIndex);
    return VKB_SUCCESS;
}

vkbResult vkbBuildAddTypeDependencies(vkbBuild &context, const char* typeName, std::vector<size_t> &typeIndicesOut, std::vector<size_t> &enumIndicesOut)
{
    if (typeName == NULL) {
        return VKB_INVALID_ARGS;
    }

    size_t typeIndex;
    if (!vkbBuildFindTypeByName(context, typeName, &typeIndex)) {
        return VKB_INVALID_ARGS;    // Couldn't find the base type
    }

    vkbBuildType &type = context.types[typeIndex];

    // If the type has an alias, make sure that's added first.
    if (type.alias != "") {
        vkbBuildAddTypeDependencies(context, type.alias.c_str(), typeIndicesOut, enumIndicesOut);
    }

    if (type.category == "define" || type.category == "basetype" || type.category == "bitmask" || type.category == "handle" || type.category == "enum") {
        if (type.type.length() > 0) {
            vkbBuildAddTypeDependencies(context, type.type.c_str(), typeIndicesOut, enumIndicesOut);
        }
        if (type.requires.length() > 0) {
            vkbBuildAddTypeDependencies(context, type.requires.c_str(), typeIndicesOut, enumIndicesOut);
        }
        if (type.bitvalues.length() > 0) {
            vkbBuildAddTypeDependencies(context, type.bitvalues.c_str(), typeIndicesOut, enumIndicesOut);
        }
    } else if (type.category == "struct" || type.category == "union") {
        for (size_t iMember = 0; iMember < type.structData.members.size(); ++iMember) {
            if (type.structData.members[iMember].type == typeName) {
                continue;   // It's the same type name. Prevent an infinite recursion loop.
            }

            if (type.structData.members[iMember].arrayEnum.length() > 0) {
                vkbBuildAddEnumDependencies(context, type.structData.members[iMember].arrayEnum.c_str(), enumIndicesOut);
            }
            vkbBuildAddTypeDependencies(context, type.structData.members[iMember].type.c_str(), typeIndicesOut, enumIndicesOut);
        }
    } else if (type.category == "funcpointer") {
        vkbBuildAddTypeDependencies(context, type.funcpointer.returnType.c_str(), typeIndicesOut, enumIndicesOut);
        for (size_t iParam = 0; iParam < type.funcpointer.params.size(); ++iParam) {
            if (type.funcpointer.params[iParam].arrayEnum.length() > 0) {
                vkbBuildAddEnumDependencies(context, type.funcpointer.params[iParam].arrayEnum.c_str(), enumIndicesOut);
            }
            vkbBuildAddTypeDependencies(context, type.funcpointer.params[iParam].type.c_str(), typeIndicesOut, enumIndicesOut);
        }
    } else if (type.category == "") {
        if (type.requires.length() > 0) {
            vkbBuildAddTypeDependencies(context, type.requires.c_str(), typeIndicesOut, enumIndicesOut);
        }
        if (type.bitvalues.length() > 0) {
            vkbBuildAddTypeDependencies(context, type.bitvalues.c_str(), typeIndicesOut, enumIndicesOut);
        }
    }

    // Getting here means we found the base type. We need to recursively add the dependencies of each referenced type. If
    // the base type is already in the list, it means it's already been handled and we should skip it.
    if (vkbContains(typeIndicesOut, typeIndex)) {
        return VKB_SUCCESS; // This type has already been handled.
    }

    // We add the base type to the list last.
    typeIndicesOut.push_back(typeIndex);
    return VKB_SUCCESS;
}

vkbResult vkbBuildAddCommandDependencies(vkbBuild &context, const char* commandName, std::vector<size_t> &typeIndicesOut, std::vector<size_t> &enumIndicesOut)
{
    if (commandName == NULL) {
        return VKB_INVALID_ARGS;
    }

    size_t commandIndex;
    if (!vkbBuildFindCommandByName(context, commandName, &commandIndex)) {
        return VKB_INVALID_ARGS;    // Couldn't find the base type
    }

    vkbBuildCommand &command = context.commands[commandIndex];
    vkbBuildAddTypeDependencies(context, command.returnType.c_str(), typeIndicesOut, enumIndicesOut);
    for (size_t iParam = 0; iParam < command.parameters.size(); ++iParam) {
        vkbBuildAddTypeDependencies(context, command.parameters[iParam].type.c_str(), typeIndicesOut, enumIndicesOut);
    }

    return VKB_SUCCESS;
}


std::string vkbBuildCalculateExtensionEnumValue(vkbBuildRequireEnum &requireEnum, const std::string &extnumber)
{
    // See "Assigning Extension Token Values" in the Vulkan style guide.
    int dir = (requireEnum.dir == "-") ? -1 : +1;
    int ext = atoi(extnumber.c_str());
    int off = atoi(requireEnum.offset.c_str());
    int val = (1000000000 + (ext-1)*1000 + off) * dir;
    return std::to_string(val);
}

std::string vkbBuildCalculateExtensionEnumValue(vkbBuildRequireEnum &requireEnum)
{
    return vkbBuildCalculateExtensionEnumValue(requireEnum, requireEnum.extnumber);
}

std::string vkbBuildBitPosToHexString(int bitpos)
{
    assert(bitpos >= 0);
    assert(bitpos <= 63);

    char buffer[32];
    if (bitpos < 32) {
        snprintf(buffer, sizeof(buffer), "0x%08x", (1 << bitpos));
    } else {
        snprintf(buffer, sizeof(buffer), "0x%016llx", ((long long)1 << bitpos));
    }

    return buffer;
}

std::string vkbBuildBitPosToHexStringEx(int bitpos, const std::string &typeName)
{
    char buffer[1024];

    assert(bitpos >= 0);
    assert(bitpos <= 63);


    if (bitpos < 32) {
        snprintf(buffer, sizeof(buffer), "0x%08x", (1 << bitpos));
    } else {
        /* Strange syntax is for VC6 compatibility. */
        long long value = ((long long)1 << bitpos);
        snprintf(buffer, sizeof(buffer), "(%s)(((%s)0x%08x << 32) | (0x%08x))", typeName.c_str(), typeName.c_str(), (int)((value & 0xFFFFFFFF00000000) >> 32), (int)(value & 0x00000000FFFFFFFF));
    }

    return buffer;
}



// Keeps track of the different types and defines that a feature or extension depends on.
struct vkbBuildCodeGenDependencies
{
    std::string type;   // "feature" or "extension".
    std::vector<size_t> typeIndexes;    // An index into the main type list.
    std::vector<size_t> enumIndexes;    // An index into the main enum list.

    vkbResult ParseFeatureDependencies(vkbBuild &context, vkbBuildFeature &feature)
    {
        type = "feature";

        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbResult result = ParseRequire(context, feature.requires[iRequire]);
            if (result != VKB_SUCCESS) {
                return result;
            }
        }
        
        return VKB_SUCCESS;
    }

    vkbResult ParseExtensionDependencies(vkbBuild &context, vkbBuildExtension &extension)
    {
        type = "extension";

        for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
            vkbResult result = ParseRequire(context, extension.requires[iRequire]);
            if (result != VKB_SUCCESS) {
                return result;
            }
        }

        return VKB_SUCCESS;
    }

    vkbResult ParseRequire(vkbBuild &context, vkbBuildRequire &require)
    {
        for (size_t iRequireType = 0; iRequireType < require.types.size(); ++iRequireType) {
            vkbBuildAddTypeDependencies(context, require.types[iRequireType].name.c_str(), typeIndexes, enumIndexes);
        }
        for (size_t iRequireEnum = 0; iRequireEnum < require.enums.size(); ++iRequireEnum) {
            vkbBuildAddEnumDependencies(context, require.enums[iRequireEnum].name.c_str(), enumIndexes);
        }
        for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
            vkbBuildAddCommandDependencies(context, require.commands[iRequireCommand].name.c_str(), typeIndexes, enumIndexes);
        }

        return VKB_SUCCESS;
    }
};

// Keeps track of what has been output from the codegen stage. This is used for keeping track 
struct vkbBuildCodeGenState
{
    std::vector<vkbBuildCodeGenDependencies> featureDependencies;
    std::vector<vkbBuildCodeGenDependencies> extensionDependencies;
    std::vector<std::string> outputDefines;     // <-- Keeps track of #define's that have already been output.
    std::vector<std::string> outputTypes;       // <-- Keeps track of types that have already been output (base types, struct, union, etc.).
    std::vector<std::string> outputCommands;    // <-- Keeps track of the commands that have already been output.

    bool HasOutputDefine(const std::string &name) const
    {
        return vkbContains(outputDefines, name);
    }

    bool HasOutputType(const std::string &name) const
    {
        return vkbContains(outputTypes, name);
    }

    bool HasOutputCommand(const std::string &name) const
    {
        return vkbContains(outputCommands, name);
    }


    void MarkDefineAsOutput(const std::string &name)
    {
        assert(!HasOutputDefine(name));
        outputDefines.push_back(name);
    }

    void MarkTypeAsOutput(const std::string &name)
    {
        assert(!HasOutputType(name));
        outputTypes.push_back(name);
    }

    void MarkCommandAsOutput(const std::string &name)
    {
        assert(!HasOutputCommand(name));
        outputCommands.push_back(name);
    }
};

vkbResult vkbBuildGenerateCode_C_DependencyIncludes(vkbBuild &context, vkbBuildCodeGenState &codegenState, const vkbBuildCodeGenDependencies &dependencies, std::string &codeOut)
{
    const std::vector<size_t> &typeIndices = dependencies.typeIndexes;

    for (size_t iType = 0; iType < typeIndices.size(); ++iType) {
        vkbBuildType &type = context.types[typeIndices[iType]];

        // vkbind doesn't want to depend on vk_platform.h so skip it.
        if (type.name == "vk_platform") {
            continue;
        }

        if (type.category == "include") {
            if (!codegenState.HasOutputType(type.name)) {
                codeOut += "#include <" + type.name + ">\n";
                codegenState.MarkTypeAsOutput(type.name);
            }
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_RequireDefineEnums(vkbBuild &context, vkbBuildCodeGenState &codegenState, const vkbBuildRequire &require, std::string &codeOut)
{
    (void)context;

    for (size_t iRequireEnum = 0; iRequireEnum < require.enums.size(); ++iRequireEnum) {
        const vkbBuildRequireEnum &requireEnum = require.enums[iRequireEnum];
        if (requireEnum.value != "" && requireEnum.extends == "") {
            if (requireEnum.alias != "") {
                codeOut += "#define " + requireEnum.name + " " + requireEnum.alias + "\n";
            } else {
                codeOut += "#define " + requireEnum.name + " " + requireEnum.value + "\n";
            }
            codegenState.MarkDefineAsOutput(requireEnum.name);
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_Function(const std::string &returnTypeC, const std::string &name, const std::vector<vkbBuildFunctionParameter> &parameters, std::string &codeOut)
{
    /* We want to prefix the name with "PFN_", but sometimes it already is, in which case we don't need to add a prefix explicitly. */
    std::string namePrefix;
    if (name.find("PFN_") == std::string::npos) {
        namePrefix += "PFN_";
    }

    codeOut += "typedef " + returnTypeC + " (VKAPI_PTR *" + namePrefix + name + ")(";
    if (parameters.size() > 0) {
        for (size_t iParam = 0; iParam < parameters.size(); ++iParam) {
            if (iParam > 0) {
                codeOut += ", ";
            }
            codeOut += parameters[iParam].typeC + " " + parameters[iParam].nameC;
        }
    } else {
        codeOut += "void";
    }
    codeOut += ");\n";

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_Command(vkbBuildCommand &command, const std::string &name, std::string &codeOut)
{
    return vkbBuildGenerateCode_C_Function(command.returnType, name, command.parameters, codeOut);
}

vkbResult vkbBuildGenerateCode_C_FuncPointer(vkbBuildFunctionPointer &funcpointer, const std::string &name, std::string &codeOut)
{
    return vkbBuildGenerateCode_C_Function(funcpointer.returnType, name, funcpointer.params, codeOut);
}

vkbResult vkbBuildGenerateCode_C_RequireCommands(vkbBuild &context, vkbBuildCodeGenState &codegenState, const std::vector<vkbBuildRequireCommand> &commands, std::string &codeOut)
{
    for (size_t iRequireCommand = 0; iRequireCommand < commands.size(); ++iRequireCommand) {
        const vkbBuildRequireCommand &requireCommand = commands[iRequireCommand];

        size_t iCommand;
        if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
            vkbBuildCommand &command = context.commands[iCommand];
            if (!codegenState.HasOutputCommand(command.name)) {
                if (command.alias != "") {
                    /*
                    It'd be nice to just use a typedef here, but we can't because I've had cases some of them are aliased with a version that's contained in VK_ENABLE_BETA_EXTENSIONS. We need to just output
                    the entire declaration.
                    */
                    size_t iBaseCommand;
                    if (vkbBuildFindCommandByName(context, command.alias.c_str(), &iBaseCommand)) {
                        vkbBuildGenerateCode_C_Command(context.commands[iBaseCommand], command.name, codeOut);
                    }
                } else {
                    vkbBuildGenerateCode_C_Command(command, command.name, codeOut);
                }
                codegenState.MarkCommandAsOutput(command.name);
            }
        }
    }

    return VKB_SUCCESS;
}

std::string vkbNameToUpperCaseStyle(const std::string &name)
{
    std::string result = "VK"; // The final result will always start with "VK".

    // Slow, but simple. We're just going to find all occurance of a capital letter and then insert an underscore. Otherwise we just convert to upper case.
    for (size_t i = 2; i < name.length(); i += 1) { // Start at 2 since it'll always start with "Vk".
        if (std::isupper(name[i])) {
            result += "_";
            result += name[i];
        } else {
            result += (char)std::toupper(name[i]);
        }
    }

    return result;
}

std::string vkbExtractTagFromName(vkbBuild &context, const std::string &name)
{
    for (auto tag : context.tags) {
        if (name.length() > tag.name.length()) {
            if (name.substr(name.length() - tag.name.length()) == tag.name) {
                return tag.name;
            }
        }
    }

    // It's not tagged.
    return "";
}

std::string vkbGenerateMaxEnumToken(vkbBuild &context, const std::string &enumName)
{
    // First thing to do is extract the tag, if any. We need to determine thi
    std::string tag = vkbExtractTagFromName(context, enumName);

    // Convert to upper case style, minus the vendor tag. The vender tag needs to be added after the _MAX_ENUM part.
    std::string result = vkbNameToUpperCaseStyle(std::string(enumName.begin(), enumName.end() - tag.length()));

    // _MAX_ENUM needs to be added. The vendor tag, if any, comes after this.
    result += "_MAX_ENUM";

    // If we have a tag we need to append it to the end.
    if (tag.length() > 0) {
        result += "_" + tag;
    }

    return result;
}

vkbResult vkbBuildGenerateCode_C_Dependencies(vkbBuild &context, vkbBuildCodeGenState &codegenState, const vkbBuildCodeGenDependencies &dependencies, std::string &codeOut)
{
    const std::vector<size_t> &typeIndices = dependencies.typeIndexes;
    const std::vector<size_t> &enumIndices = dependencies.enumIndexes;

    // define
    {
        uint32_t count = 0;
        for (size_t iType = 0; iType < typeIndices.size(); ++iType) {
            vkbBuildType &type = context.types[typeIndices[iType]];
            if (!codegenState.HasOutputDefine(type.name)) {
                if (type.category == "define") {
                    std::string defineValue = vkbBuildCleanDefineValue(type.verbatimValue);
                    if (defineValue.size() > 0) {
                        codeOut += defineValue + "\n";
                        count += 1;
                        codegenState.MarkDefineAsOutput(type.name);
                    }
                }
            }
        }
        if (count > 0) { codeOut += "\n"; }

        // #define-style enums.
        count = 0;
        for (size_t iEnum = 0; iEnum < enumIndices.size(); ++iEnum) {
            vkbBuildEnums &enums = context.enums[enumIndices[iEnum]];
            if (!codegenState.HasOutputDefine(enums.enums[0].name)) {
                if (enums.type == "") {
                    if (enums.enums[0].alias != "") {
                        codeOut += "#define " + enums.enums[0].name + " " + enums.enums[0].alias + "\n";
                    } else {
                        codeOut += "#define " + enums.enums[0].name + " " + enums.enums[0].value + "\n";
                    }
                    count += 1;
                    codegenState.MarkDefineAsOutput(enums.enums[0].name);
                }
            }
        }
        if (count > 0) { codeOut += "\n"; }
    }

    // basetype
    {
        uint32_t count = 0;
        for (size_t iType = 0; iType < typeIndices.size(); ++iType) {
            vkbBuildType &type = context.types[typeIndices[iType]];
            if (!codegenState.HasOutputType(type.name)) {
                if (type.category == "basetype") {
                    codeOut += type.verbatimValue + "\n";
                    count += 1;
                    codegenState.MarkTypeAsOutput(type.name);
                }
            }
        }
        if (count > 0) { codeOut += "\n"; }
    }

    // handle
    {
        uint32_t count = 0;
        for (size_t iType = 0; iType < typeIndices.size(); ++iType) {
            vkbBuildType &type = context.types[typeIndices[iType]];
            if (!codegenState.HasOutputType(type.name)) {
                if (type.category == "handle") {
                    if (type.alias != "") {
                        codeOut += "typedef " + type.alias + " " + type.name + ";\n";
                    } else {
                        codeOut += type.type + "(" + type.name + ")\n";
                        count += 1;
                    }
                    codegenState.MarkTypeAsOutput(type.name);
                }
            }
        }
        if (count > 0) { codeOut += "\n"; }
    }


    // NOTE: bitmask and enum types must be done in the same iteration because there's been times where an aliased bitmask or enum is typed differently to it's aliased type.

    // bitmask and enum.
    {
        uint32_t count = 0;
        for (size_t iType = 0; iType < typeIndices.size(); ++iType) {
            vkbBuildType &type = context.types[typeIndices[iType]];
            if (!codegenState.HasOutputType(type.name)) {
                if (type.category == "bitmask" || type.category == "enum") {
                    if (type.alias != "") {
                        codeOut += "typedef " + type.alias + " " + type.name + ";\n";
                    } else {
                        if (type.category == "bitmask") {
                            if (type.requires.length() > 0 || type.bitvalues.length() > 0) {
                                size_t iEnums;
                                bool enumsFound;
                                
                                if (type.requires.length() > 0) {
                                    enumsFound = vkbBuildFindEnumByName(context, type.requires.c_str(), &iEnums);
                                } else {
                                    assert(type.bitvalues.length() > 0);
                                    enumsFound = vkbBuildFindEnumByName(context, type.bitvalues.c_str(), &iEnums);
                                }

                                if (enumsFound) {
                                    vkbBuildEnums &enums = context.enums[iEnums];
                                    uint32_t enumValueCount = 0;
                                    std::vector<std::string> outputEnums;
                                    std::string enumValuePrefix;
                                    bool using64BitFlags;

                                    codeOut += '\n';

                                    if (type.bitvalues.length() == 0) {
                                        /* 32-bit enums. Use an enum. */
                                        using64BitFlags = false;
                                        codeOut += "typedef enum\n";
                                        codeOut += "{\n";
                                        enumValuePrefix = "    ";
                                    } else {
                                        /* 64-bit enums. Cannot use an enum. */
                                        using64BitFlags = true;
                                        codeOut += "typedef "; codeOut += type.type; codeOut += " "; codeOut += enums.name; codeOut += ";\n";
                                        enumValuePrefix = "static const "; enumValuePrefix += enums.name; enumValuePrefix += " ";
                                    }
                                    
                                    for (size_t iEnumValue = 0; iEnumValue < enums.enums.size(); ++iEnumValue) {
                                        if (!using64BitFlags && iEnumValue > 0) {
                                            codeOut += ",\n";
                                        }

                                        /*
                                        When outputting 64-bit flags we can't assign to aliased types in case some compilers
                                        complain about it not being const. We therefore need to evaluate the aliased types.
                                        */
                                        vkbBuildEnum enumValue;
                                        if (using64BitFlags && enums.enums[iEnumValue].alias != "") {
                                            if (!vkbBuildFindEnumValue(context, enums.enums[iEnumValue].alias, enumValue)) {
                                                /* Couldn't find the aliased type. */
                                                enumValue = enums.enums[iEnumValue];
                                            }
                                        } else {
                                            enumValue = enums.enums[iEnumValue];
                                        }

                                        if (enumValue.bitpos.length() > 0) {
                                            codeOut += enumValuePrefix + enums.enums[iEnumValue].name + " = " + vkbBuildBitPosToHexStringEx(atoi(enumValue.bitpos.c_str()), enums.name);
                                        } else {
                                            if (enumValue.alias != "") {
                                                codeOut += enumValuePrefix + enums.enums[iEnumValue].name + " = " + enumValue.alias;
                                            } else {
                                                codeOut += enumValuePrefix + enums.enums[iEnumValue].name + " = " + enumValue.value;
                                            }
                                        }
                                        if (using64BitFlags) {
                                            codeOut += ";\n";
                                        }
                                        outputEnums.push_back(enums.enums[iEnumValue].name);

                                        enumValueCount += 1;
                                    }

                                    // Check if any features extend the enum.
                                    for (size_t iOtherFeature = 0; iOtherFeature < context.features.size(); ++iOtherFeature) {
                                        vkbBuildFeature &otherFeature = context.features[iOtherFeature];
                                        for (size_t iOtherRequire = 0; iOtherRequire < otherFeature.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = otherFeature.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias == "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        if (!using64BitFlags && enumValueCount > 0) {
                                                            codeOut += ",\n";
                                                        }
                                                        if (otherRequireEnum.bitpos.length() > 0) {
                                                            codeOut += enumValuePrefix + otherRequireEnum.name + " = " + vkbBuildBitPosToHexStringEx(atoi(otherRequireEnum.bitpos.c_str()), otherRequireEnum.extends);
                                                        } else {
                                                            codeOut += enumValuePrefix + otherRequireEnum.name + " = " + otherRequireEnum.value;
                                                        }
                                                        if (using64BitFlags) {
                                                            codeOut += ";\n";
                                                        }
                                                        outputEnums.push_back(otherRequireEnum.name);
                                                    
                                                        enumValueCount += 1;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                                        vkbBuildExtension &extension = context.extensions[iExtension];
                                        for (size_t iOtherRequire = 0; iOtherRequire < extension.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = extension.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias == "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        if (!using64BitFlags && enumValueCount > 0) {
                                                            codeOut += ",\n";
                                                        }
                                                        if (otherRequireEnum.bitpos.length() > 0) {
                                                            codeOut += enumValuePrefix + otherRequireEnum.name + " = " + vkbBuildBitPosToHexStringEx(atoi(otherRequireEnum.bitpos.c_str()), otherRequireEnum.extends);
                                                        } else {
                                                            codeOut += enumValuePrefix + otherRequireEnum.name + " = " + otherRequireEnum.value;
                                                        }
                                                        if (using64BitFlags) {
                                                            codeOut += ";\n";
                                                        }
                                                        outputEnums.push_back(otherRequireEnum.name);

                                                        enumValueCount += 1;
                                                    }
                                                }
                                            }
                                        }
                                    }


                                    // Aliased enums.
                                    for (size_t iOtherFeature = 0; iOtherFeature < context.features.size(); ++iOtherFeature) {
                                        vkbBuildFeature &otherFeature = context.features[iOtherFeature];
                                        for (size_t iOtherRequire = 0; iOtherRequire < otherFeature.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = otherFeature.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias != "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        if (!using64BitFlags && enumValueCount > 0) {
                                                            codeOut += ",\n";
                                                        }

                                                        if (using64BitFlags) {
                                                            vkbBuildEnum enumValue;
                                                            if (vkbBuildFindEnumValue(context, otherRequireEnum.alias, enumValue)) {
                                                                if (enumValue.bitpos.length() > 0) {
                                                                    codeOut += enumValuePrefix + otherRequireEnum.name + " = " + vkbBuildBitPosToHexStringEx(atoi(enumValue.bitpos.c_str()), otherRequireEnum.extends);
                                                                } else {
                                                                    codeOut += enumValuePrefix + otherRequireEnum.name + " = " + enumValue.value;
                                                                }

                                                                codeOut += ";\n";
                                                            } else {
                                                                /* Couldn't find the aliased type. */
                                                                codeOut += enumValuePrefix + otherRequireEnum.name + " = " + otherRequireEnum.alias + ";\n";
                                                            }
                                                        } else {
                                                            codeOut += enumValuePrefix + otherRequireEnum.name + " = " + otherRequireEnum.alias;
                                                        }
                                                        
                                                        outputEnums.push_back(otherRequireEnum.name);
                                                        enumValueCount += 1;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                                        vkbBuildExtension &extension = context.extensions[iExtension];
                                        for (size_t iOtherRequire = 0; iOtherRequire < extension.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = extension.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias != "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        if (!using64BitFlags && enumValueCount > 0) {
                                                            codeOut += ",\n";
                                                        }

                                                        if (using64BitFlags) {
                                                            vkbBuildEnum enumValue;
                                                            if (vkbBuildFindEnumValue(context, otherRequireEnum.alias, enumValue)) {
                                                                if (enumValue.bitpos.length() > 0) {
                                                                    codeOut += enumValuePrefix + otherRequireEnum.name + " = " + vkbBuildBitPosToHexStringEx(atoi(enumValue.bitpos.c_str()), otherRequireEnum.extends);
                                                                } else {
                                                                    codeOut += enumValuePrefix + otherRequireEnum.name + " = " + enumValue.value;
                                                                }

                                                                codeOut += ";\n";
                                                            } else {
                                                                /* Couldn't find the aliased type. */
                                                                codeOut += enumValuePrefix + otherRequireEnum.name + " = " + otherRequireEnum.alias + ";\n";
                                                            }
                                                        } else {
                                                            codeOut += enumValuePrefix + otherRequireEnum.name + " = " + otherRequireEnum.alias;
                                                        }

                                                        outputEnums.push_back(otherRequireEnum.name);
                                                        enumValueCount += 1;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    /* We need to do the _ENUM_MAX[_VENDOR] part. */
                                    if (!using64BitFlags) {
                                        if (enumValueCount > 0) {
                                            codeOut += ",\n";
                                        }
                                        codeOut += "    " + vkbGenerateMaxEnumToken(context, enums.name) + " = 0x7FFFFFFF";

                                        codeOut += "\n} " + enums.name + ";\n";
                                    }

                                    count += 1;
                                }
                            }

                            codeOut += "typedef " + type.type + " " + type.name + ";\n";
                        } /* bitmask */

                        if (type.category == "enum") {
                            size_t iEnums;
                            if (vkbBuildFindEnumByName(context, type.name.c_str(), &iEnums)) {
                                vkbBuildEnums &enums = context.enums[iEnums];
                                if (enums.type == "enum") {
                                    std::vector<std::string> outputEnums;

                                    codeOut += "typedef enum\n";
                                    codeOut += "{\n";
                                    for (size_t iEnumValue = 0; iEnumValue < enums.enums.size(); ++iEnumValue) {
                                        if (iEnumValue > 0) {
                                            codeOut += ",\n";
                                        }
                                        if (enums.enums[iEnumValue].alias != "") {
                                            codeOut += "    " + enums.enums[iEnumValue].name + " = " + enums.enums[iEnumValue].alias;
                                        } else {
                                            codeOut += "    " + enums.enums[iEnumValue].name + " = " + enums.enums[iEnumValue].value;
                                        }
                                        outputEnums.push_back(enums.enums[iEnumValue].name);
                                    }

                                    // For cleanliness, this is done in two passes so that aliased types are at the bottom of the enum.

                                    // We need to check other features and extensions in case we need to extend this enum.
                                    for (size_t iOtherFeature = 0; iOtherFeature < context.features.size(); ++iOtherFeature) {
                                        vkbBuildFeature &otherFeature = context.features[iOtherFeature];
                                        for (size_t iOtherRequire = 0; iOtherRequire < otherFeature.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = otherFeature.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias == "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        codeOut += ",\n";
                                                        if (otherRequireEnum.value != "") {
                                                            codeOut += "    " + otherRequireEnum.name + " = " + otherRequireEnum.value;
                                                        } else {
                                                            codeOut += "    " + otherRequireEnum.name + " = " + vkbBuildCalculateExtensionEnumValue(otherRequireEnum);
                                                        }
                                                        
                                                        outputEnums.push_back(otherRequireEnum.name);
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Same thing for extensions.
                                    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                                        vkbBuildExtension &extension = context.extensions[iExtension];
                                        for (size_t iOtherRequire = 0; iOtherRequire < extension.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = extension.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias == "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        codeOut += ",\n";
                                                        if (otherRequireEnum.value != "") {
                                                            codeOut += "    " + otherRequireEnum.name + " = " + otherRequireEnum.value;
                                                        } else {
                                                            codeOut += "    " + otherRequireEnum.name + " = " + vkbBuildCalculateExtensionEnumValue(otherRequireEnum, (otherRequireEnum.extnumber != "") ? otherRequireEnum.extnumber : extension.number);
                                                        }
                                                        outputEnums.push_back(otherRequireEnum.name);
                                                    }
                                                }
                                            }
                                        }
                                    }


                                    // Aliased enum values.
                                    for (size_t iOtherFeature = 0; iOtherFeature < context.features.size(); ++iOtherFeature) {
                                        vkbBuildFeature &otherFeature = context.features[iOtherFeature];
                                        for (size_t iOtherRequire = 0; iOtherRequire < otherFeature.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = otherFeature.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias != "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        codeOut += ",\n";
                                                        codeOut += "    " + otherRequireEnum.name + " = " + otherRequireEnum.alias;
                                                        outputEnums.push_back(otherRequireEnum.name);
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                                        vkbBuildExtension &extension = context.extensions[iExtension];
                                        for (size_t iOtherRequire = 0; iOtherRequire < extension.requires.size(); ++iOtherRequire) {
                                            vkbBuildRequire &otherRequire = extension.requires[iOtherRequire];
                                            for (size_t iOtherEnum = 0; iOtherEnum < otherRequire.enums.size(); ++iOtherEnum) {
                                                vkbBuildRequireEnum &otherRequireEnum = otherRequire.enums[iOtherEnum];
                                                if (otherRequireEnum.extends == enums.name) {
                                                    if (otherRequireEnum.alias != "" && !vkbContains(outputEnums, otherRequireEnum.name)) {
                                                        codeOut += ",\n";
                                                        codeOut += "    " + otherRequireEnum.name + " = " + otherRequireEnum.alias;
                                                        outputEnums.push_back(otherRequireEnum.name);
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    /* We need to do the _ENUM_MAX[_VENDOR] part. */
                                    codeOut += ",\n";
                                    codeOut += "    " + vkbGenerateMaxEnumToken(context, enums.name) + " = 0x7FFFFFFF";

                                    codeOut += "\n} " + enums.name + ";\n\n";
                                    count += 1;
                                }
                            }
                        }
                    }

                    codegenState.MarkTypeAsOutput(type.name);
                }
            }
        }
        if (count > 0) { codeOut += "\n"; }
    }


    // struct, unions and and funcpointer. There's an unfortunate complication in that some function pointers may depend on structures,
    // and some structures may depend on function pointers. All we do is make sure we group the output of each of these types into the
    // same iteration and it should just work.
    {
        bool wasFuncPointerOutputLast = false;
        uint32_t count = 0;
        for (size_t iType = 0; iType < typeIndices.size(); ++iType) {
            vkbBuildType &type = context.types[typeIndices[iType]];
            if (!codegenState.HasOutputType(type.name)) {
                if (type.category == "struct" || type.category == "union") {
                    if (type.alias != "") {
                        codeOut += "typedef " + type.alias + " " + type.name + ";\n\n";
                    } else {
                        if (wasFuncPointerOutputLast) {
                            codeOut += "\n";    // Separate function pointers and struct/union types with a space.
                        }
                        codeOut += "typedef " + type.category + " " + type.name + "\n";
                        codeOut += "{\n";
                        for (size_t iMember = 0; iMember < type.structData.members.size(); ++iMember) {
                            codeOut += "    " + type.structData.members[iMember].typeC + " " + type.structData.members[iMember].nameC + ";\n";
                        }
                        codeOut += "} " + type.name + ";\n\n";
                        count += 1;
                    }
                    codegenState.MarkTypeAsOutput(type.name);
                    wasFuncPointerOutputLast = false;
                }

                if (type.category == "funcpointer") {
                    if (type.alias != "") {
                        /*
                        It'd be nice to just use a typedef here, but we can't because I've had cases some of them are aliased with a version that's contained in VK_ENABLE_BETA_EXTENSIONS. We need to just output
                        the entire declaration.
                        */
                        size_t iBaseType;
                        if (vkbBuildFindTypeByName(context, type.alias.c_str(), &iBaseType)) {
                            vkbBuildGenerateCode_C_FuncPointer(context.types[iBaseType].funcpointer, type.name, codeOut);
                            count += 1;
                        }
                    } else {
                        vkbBuildGenerateCode_C_FuncPointer(type.funcpointer, type.name, codeOut);
                        count += 1;
                    }
                    codegenState.MarkTypeAsOutput(type.name);
                    wasFuncPointerOutputLast = true;
                }
            }
        }
        if (count > 0) { codeOut += "\n"; }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_Feature(vkbBuild &context, vkbBuildCodeGenState &codegenState, size_t iFeature, std::string &codeOut)
{
    codeOut += "\n#define " + context.features[iFeature].name + " 1\n";

    // Includes.
    vkbResult result = vkbBuildGenerateCode_C_DependencyIncludes(context, codegenState, codegenState.featureDependencies[iFeature], codeOut);
    if (result != VKB_SUCCESS) {
        return result;
    }

    // #define-style enums within each require tag.
    {
        for (size_t iRequire = 0; iRequire < context.features[iFeature].requires.size(); ++iRequire) {
            vkbBuildRequire &require = context.features[iFeature].requires[iRequire];
            result = vkbBuildGenerateCode_C_RequireDefineEnums(context, codegenState, require, codeOut);
            if (result != VKB_SUCCESS) {
                return result;
            }
        }
        codeOut += "\n";
    }

    result = vkbBuildGenerateCode_C_Dependencies(context, codegenState, codegenState.featureDependencies[iFeature], codeOut);
    if (result != VKB_SUCCESS) {
        return result;
    }

    // command
    {
        for (size_t iRequire = 0; iRequire < context.features[iFeature].requires.size(); ++iRequire) {
            vkbBuildRequire &require = context.features[iFeature].requires[iRequire];
            result = vkbBuildGenerateCode_C_RequireCommands(context, codegenState, require.commands, codeOut);
            if (result != VKB_SUCCESS) {
                return result;
            }
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_Extension(vkbBuild &context, vkbBuildCodeGenState &codegenState, size_t iExtension, std::string &codeOut)
{
    vkbBuildExtension &extension = context.extensions[iExtension];

    // Extension-specific stuff. Extensions can define enums in #define style directly within the <require> tag. These need to be handled here.
    codeOut += "\n#define " + extension.name + " 1\n";

    // Includes.
    vkbResult result = vkbBuildGenerateCode_C_DependencyIncludes(context, codegenState, codegenState.extensionDependencies[iExtension], codeOut);
    if (result != VKB_SUCCESS) {
        return result;
    }

    // #define-style enums within each require tag.
    {
        for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
            vkbBuildRequire &require = extension.requires[iRequire];
            result = vkbBuildGenerateCode_C_RequireDefineEnums(context, codegenState, require, codeOut);
            if (result != VKB_SUCCESS) {
                return result;
            }
        }
        codeOut += "\n";
    }

    result = vkbBuildGenerateCode_C_Dependencies(context, codegenState, codegenState.extensionDependencies[iExtension], codeOut);
    if (result != VKB_SUCCESS) {
        return result;
    }

    // command
    {
        for (size_t iRequire = 0; iRequire < context.extensions[iExtension].requires.size(); ++iRequire) {
            vkbBuildRequire &require = context.extensions[iExtension].requires[iRequire];
            result = vkbBuildGenerateCode_C_RequireCommands(context, codegenState, require.commands, codeOut);
            if (result != VKB_SUCCESS) {
                return result;
            }
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildReorderExtensions(vkbBuild &context)
{
    std::vector<vkbBuildExtension> promotedExtensions;

    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.promotedto != "") {
            promotedExtensions.push_back(extension);
        }
    }

    // At this point we have the promoted extensions, so now we need to remove them and then reinsert them after their promoted equivalent.
    for (size_t iPromotedExtension = 0; iPromotedExtension < promotedExtensions.size(); ++iPromotedExtension) {
        size_t iOldIndex;
        size_t iNewIndex;
        if (vkbBuildFindExtensionByName(context, promotedExtensions[iPromotedExtension].name.c_str(),       &iOldIndex) &&
            vkbBuildFindExtensionByName(context, promotedExtensions[iPromotedExtension].promotedto.c_str(), &iNewIndex)) {
            context.extensions.erase( context.extensions.begin() + iOldIndex);
            context.extensions.insert(context.extensions.begin() + iNewIndex, promotedExtensions[iPromotedExtension]);
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_Main(vkbBuild &context, std::string &codeOut)
{
    vkbResult result;
    vkbBuildCodeGenState codegenState;

    // We need to reorder extensions so that any that have been promoted are located _after_ the promoted extension.
    result = vkbBuildReorderExtensions(context);
    if (result != VKB_SUCCESS) {
        return result;
    }
    

    // The first thing to do is extract all of the dependencies for each feature and extension.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildCodeGenDependencies dependencies;
        result = dependencies.ParseFeatureDependencies(context, context.features[iFeature]);
        if (result != VKB_SUCCESS) {
            return result;
        }

        codegenState.featureDependencies.push_back(dependencies);
    }

    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildCodeGenDependencies dependencies;
        result = dependencies.ParseExtensionDependencies(context, context.extensions[iExtension]);
        if (result != VKB_SUCCESS) {
            return result;
        }

        codegenState.extensionDependencies.push_back(dependencies);
    }


    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        result = vkbBuildGenerateCode_C_Feature(context, codegenState, iFeature, codeOut);
        if (result != VKB_SUCCESS) {
            return result;
        }
    }

    // Cross-platform extensions.
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.platform == "") {
            result = vkbBuildGenerateCode_C_Extension(context, codegenState, iExtension, codeOut);
            if (result != VKB_SUCCESS) {
                return result;
            }
        }
    }

    // Platform-specific extensions. We do this per-platform.
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        vkbBuildPlatform &platform = context.platforms[iPlatform];

        // Support for Mir is being dropped from Vulkan. Skip it.
        if (platform.name == "mir") {
            continue;
        }
        
        codeOut += "#ifdef " + platform.protect + "\n";
        {
            // I want platform-specific #include's to be the first thing inside the platform guard just for aesthetic reasons. Therefore, the
            // "include" parts are done slightly differently for platform-specific extensions.
            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                if (context.extensions[iExtension].platform == platform.name) {
                    result = vkbBuildGenerateCode_C_DependencyIncludes(context, codegenState, codegenState.extensionDependencies[iExtension], codeOut);
                    if (result != VKB_SUCCESS) {
                        return result;
                    }
                }
            }

            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                vkbBuildExtension &extension = context.extensions[iExtension];
                if (extension.platform == platform.name) {
                    result = vkbBuildGenerateCode_C_Extension(context, codegenState, iExtension, codeOut);
                    if (result != VKB_SUCCESS) {
                        return result;
                    }
                }
            }
        }
        codeOut += "#endif /*" + platform.protect + "*/\n\n";
    }


    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_FuncPointersDeclGlobal(vkbBuild &context, int indentation, std::string &codeOut)
{
    // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
    std::vector<std::string> outputCommands;

    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];
        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                size_t iCommand;
                if (vkbBuildFindCommandByName(context, require.commands[iRequireCommand].name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, require.commands[iRequireCommand].name)) {
                        // New line if required.
                        if (outputCommands.size() > 0) {
                            codeOut += "\n";
                            for (int iSpace = 0; iSpace < indentation; ++iSpace) {
                                codeOut += " ";
                            }
                        }
                        codeOut += "PFN_" + context.commands[iCommand].name + " " + context.commands[iCommand].name + ";";

                        outputCommands.push_back(require.commands[iRequireCommand].name);
                    }
                }
            }
        }
    }

    // Platform-independent extensions.
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.platform == "") {
            for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                vkbBuildRequire &require = extension.requires[iRequire];
                for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                    vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                    size_t iCommand;
                    if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                        if (!vkbContains(outputCommands, requireCommand.name)) {
                            // New line if required.
                            if (outputCommands.size() > 0) {
                                codeOut += "\n";
                                for (int iSpace = 0; iSpace < indentation; ++iSpace) {
                                    codeOut += " ";
                                }
                            }
                            codeOut += "PFN_" + context.commands[iCommand].name + " " + context.commands[iCommand].name + ";";

                            outputCommands.push_back(requireCommand.name);
                        }
                    }
                }
            }
        }
    }

    // Platform-specific extensions.
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        vkbBuildPlatform &platform = context.platforms[iPlatform];
        codeOut += "\n#ifdef " + platform.protect;
        {
            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                vkbBuildExtension &extension = context.extensions[iExtension];
                if (extension.platform == platform.name) {
                    for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                        vkbBuildRequire &require = extension.requires[iRequire];
                        for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                            vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                            size_t iCommand;
                            if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                                if (!vkbContains(outputCommands, requireCommand.name)) {
                                    // New line if required.
                                    if (outputCommands.size() > 0) {
                                        codeOut += "\n";
                                        for (int iSpace = 0; iSpace < indentation; ++iSpace) {
                                            codeOut += " ";
                                        }
                                    }
                                    codeOut += "PFN_" + context.commands[iCommand].name + " " + context.commands[iCommand].name + ";";

                                    outputCommands.push_back(requireCommand.name);
                                }
                            }
                        }
                    }
                }
            }
        }
        codeOut += "\n#endif /*" + platform.protect + "*/";
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_LoadGlobalAPIFuncPointers(vkbBuild &context, std::string &codeOut)
{
    // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
    std::vector<std::string> outputCommands;

    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];
        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                size_t iCommand;
                if (vkbBuildFindCommandByName(context, require.commands[iRequireCommand].name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, require.commands[iRequireCommand].name)) {
                        // New line if required.
                        if (outputCommands.size() > 0) {
                            codeOut += "\n    ";
                        }
                        codeOut += context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")vkb_dlsym(g_vkbVulkanSO, \"" + context.commands[iCommand].name + "\");";

                        outputCommands.push_back(require.commands[iRequireCommand].name);
                    }
                }
            }
        }
    }

    // Platform-independent extensions.
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.platform == "") {
            for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                vkbBuildRequire &require = extension.requires[iRequire];
                for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                    vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                    size_t iCommand;
                    if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                        if (!vkbContains(outputCommands, requireCommand.name)) {
                            // New line if required.
                            if (outputCommands.size() > 0) {
                                codeOut += "\n    ";
                            }
                            codeOut += context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")vkb_dlsym(g_vkbVulkanSO, \"" + context.commands[iCommand].name + "\");";
                            outputCommands.push_back(requireCommand.name);
                        }
                    }
                }
            }
        }
    }

    // Platform-specific extensions.
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        vkbBuildPlatform &platform = context.platforms[iPlatform];
        codeOut += "\n#ifdef " + platform.protect;
        {
            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                vkbBuildExtension &extension = context.extensions[iExtension];
                if (extension.platform == platform.name) {
                    for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                        vkbBuildRequire &require = extension.requires[iRequire];
                        for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                            vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                            size_t iCommand;
                            if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                                if (!vkbContains(outputCommands, requireCommand.name)) {
                                    // New line if required.
                                    if (outputCommands.size() > 0) {
                                        codeOut += "\n    ";
                                    }
                                    codeOut += context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")vkb_dlsym(g_vkbVulkanSO, \"" + context.commands[iCommand].name + "\");";

                                    outputCommands.push_back(requireCommand.name);
                                }
                            }
                        }
                    }
                }
            }
        }
        codeOut += "\n#endif /*" + platform.protect + "*/";
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_SetStructAPIFromGlobal(vkbBuild &context, std::string &codeOut)
{
    // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
    std::vector<std::string> outputCommands;

    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];
        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                size_t iCommand;
                if (vkbBuildFindCommandByName(context, require.commands[iRequireCommand].name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, require.commands[iRequireCommand].name)) {
                        // New line if required.
                        if (outputCommands.size() > 0) {
                            codeOut += "\n        ";
                        }
                        codeOut += "pAPI->" + context.commands[iCommand].name + " = " + context.commands[iCommand].name + ";";

                        outputCommands.push_back(require.commands[iRequireCommand].name);
                    }
                }
            }
        }
    }

    // Platform-independent extensions.
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.platform == "") {
            for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                vkbBuildRequire &require = extension.requires[iRequire];
                for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                    vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                    size_t iCommand;
                    if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                        if (!vkbContains(outputCommands, requireCommand.name)) {
                            // New line if required.
                            if (outputCommands.size() > 0) {
                                codeOut += "\n        ";
                            }
                            codeOut += "pAPI->" + context.commands[iCommand].name + " = " + context.commands[iCommand].name + ";";

                            outputCommands.push_back(requireCommand.name);
                        }
                    }
                }
            }
        }
    }

    // Platform-specific extensions.
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        vkbBuildPlatform &platform = context.platforms[iPlatform];
        codeOut += "\n#ifdef " + platform.protect;
        {
            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                vkbBuildExtension &extension = context.extensions[iExtension];
                if (extension.platform == platform.name) {
                    for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                        vkbBuildRequire &require = extension.requires[iRequire];
                        for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                            vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                            size_t iCommand;
                            if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                                if (!vkbContains(outputCommands, requireCommand.name)) {
                                    // New line if required.
                                    if (outputCommands.size() > 0) {
                                        codeOut += "\n        ";
                                    }
                                    codeOut += "pAPI->" + context.commands[iCommand].name + " = " + context.commands[iCommand].name + ";";

                                    outputCommands.push_back(requireCommand.name);
                                }
                            }
                        }
                    }
                }
            }
        }
        codeOut += "\n#endif /*" + platform.protect + "*/";
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_SetGlobalAPIFromStruct(vkbBuild &context, std::string &codeOut)
{
    // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
    std::vector<std::string> outputCommands;

    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];
        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                size_t iCommand;
                if (vkbBuildFindCommandByName(context, require.commands[iRequireCommand].name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, require.commands[iRequireCommand].name)) {
                        // New line if required.
                        if (outputCommands.size() > 0) {
                            codeOut += "\n    ";
                        }
                        codeOut += context.commands[iCommand].name + " = pAPI->" + context.commands[iCommand].name + ";";

                        outputCommands.push_back(require.commands[iRequireCommand].name);
                    }
                }
            }
        }
    }

    // Platform-independent extensions.
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.platform == "") {
            for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                vkbBuildRequire &require = extension.requires[iRequire];
                for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                    vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                    size_t iCommand;
                    if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                        if (!vkbContains(outputCommands, requireCommand.name)) {
                            // New line if required.
                            if (outputCommands.size() > 0) {
                                codeOut += "\n    ";
                            }
                            codeOut += context.commands[iCommand].name + " = pAPI->" + context.commands[iCommand].name + ";";

                            outputCommands.push_back(requireCommand.name);
                        }
                    }
                }
            }
        }
    }

    // Platform-specific extensions.
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        vkbBuildPlatform &platform = context.platforms[iPlatform];
        codeOut += "\n#ifdef " + platform.protect;
        {
            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                vkbBuildExtension &extension = context.extensions[iExtension];
                if (extension.platform == platform.name) {
                    for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                        vkbBuildRequire &require = extension.requires[iRequire];
                        for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                            vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                            size_t iCommand;
                            if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                                if (!vkbContains(outputCommands, requireCommand.name)) {
                                    // New line if required.
                                    if (outputCommands.size() > 0) {
                                        codeOut += "\n    ";
                                    }
                                    codeOut += context.commands[iCommand].name + " = pAPI->" + context.commands[iCommand].name + ";";

                                    outputCommands.push_back(requireCommand.name);
                                }
                            }
                        }
                    }
                }
            }
        }
        codeOut += "\n#endif /*" + platform.protect + "*/";
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_LoadInstanceAPI(vkbBuild &context, std::string &codeOut)
{
    // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
    std::vector<std::string> outputCommands;
    outputCommands.push_back("vkGetInstanceProcAddr");  // <-- Don't do vkGetInstanceProcAddr here. This is done in the template in a special way.

    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];
        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                size_t iCommand;
                if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, requireCommand.name)) {
                        if (outputCommands.size() > 1) {
                            codeOut += "\n    ";
                        }
                        codeOut += "pAPI->" + context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")vkGetInstanceProcAddr(instance, \"" + context.commands[iCommand].name + "\");";
                        outputCommands.push_back(requireCommand.name);
                    }
                }
            }
        }
    }

    // Platform-independent extensions.
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.platform == "") {
            for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                vkbBuildRequire &require = extension.requires[iRequire];
                for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                    vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                    size_t iCommand;
                    if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                        if (!vkbContains(outputCommands, requireCommand.name)) {
                            if (outputCommands.size() > 1) {
                                codeOut += "\n    ";
                            }
                            codeOut += "pAPI->" + context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")vkGetInstanceProcAddr(instance, \"" + context.commands[iCommand].name + "\");";
                            outputCommands.push_back(requireCommand.name);
                        }
                    }
                }
            }
        }
    }

    // Platform-specific extensions.
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        vkbBuildPlatform &platform = context.platforms[iPlatform];
        codeOut += "\n#ifdef " + platform.protect;
        {
            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                vkbBuildExtension &extension = context.extensions[iExtension];
                if (extension.platform == platform.name) {
                    for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                        vkbBuildRequire &require = extension.requires[iRequire];
                        for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                            vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                            size_t iCommand;
                            if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                                if (!vkbContains(outputCommands, requireCommand.name)) {
                                    if (outputCommands.size() > 1) {
                                        codeOut += "\n    ";
                                    }
                                    codeOut += "pAPI->" + context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")vkGetInstanceProcAddr(instance, \"" + context.commands[iCommand].name + "\");";
                                    outputCommands.push_back(requireCommand.name);
                                }
                            }
                        }
                    }
                }
            }
        }
        codeOut += "\n#endif /*" + platform.protect + "*/";
    }

    return VKB_SUCCESS;
}

bool vkbBuildIsTypeChildOf(vkbBuild &context, const std::string &parentType, const std::string &childType)
{
    if (parentType == childType) {
        return false;   // It's the same type, so is therefore not a child.
    }

    for (size_t iType = 0; iType < context.types.size(); ++iType) {
        vkbBuildType &type = context.types[iType];
        if (type.name == childType) {
            if (type.category == "handle") {
                if (type.parent == parentType) {
                    return true;
                } else {
                    return vkbBuildIsTypeChildOf(context, parentType, type.parent);
                }
            } else {
                return false;   // <-- It's not a handle which means it's not a child of anything.
            }
        }
    }

    // Getting here means we couldn't find the child type at all. Assume it is not a child of anything.
    return false;
}

std::string vkbBuildGetDeviceProcAddressGetterByType(vkbBuild &context, const std::string &type)
{
    // If the type is VkDevice or a child of it, we use vkGetDeviceProcAddr(). Otherwise, we use vkGetInstanceProcAddr().
    if (type == "VkDevice" || vkbBuildIsTypeChildOf(context, "VkDevice", type)) {
        return "vkGetDeviceProcAddr";
    } else {
        return "vkGetInstanceProcAddr";
    }
}

std::string vkbBuildGetDeviceProcAddressGetterByCommand(vkbBuild &context, vkbBuildCommand &command)
{
    if (command.alias == "") {
        if (command.parameters.size() > 0) {
            return vkbBuildGetDeviceProcAddressGetterByType(context, command.parameters[0].type);
        } else {
            return "vkGetInstanceProcAddr";
        }
    } else {
        // It's aliased. We need to call this based on the aliased command.
        for (size_t iCommand = 0; iCommand < context.commands.size(); ++iCommand) {
            if (context.commands[iCommand].name == command.alias) {
                return vkbBuildGetDeviceProcAddressGetterByCommand(context, context.commands[iCommand]);
            }
        }

        // Getting here means we couldn't find the aliased command. Should not get here.
        return "vkGetInstanceProcAddr";
    }
}

bool vkbBuildIsDeviceLevelCommand(vkbBuild &context, vkbBuildCommand &command)
{
    if (command.alias == "") {
        if (command.parameters.size() > 0) {
            if (command.parameters[0].type == "VkDevice" || vkbBuildIsTypeChildOf(context, "VkDevice", command.parameters[0].type)) {
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        // It's aliased. We need to call this based on the aliased command.
        for (size_t iCommand = 0; iCommand < context.commands.size(); ++iCommand) {
            if (context.commands[iCommand].name == command.alias) {
                return vkbBuildIsDeviceLevelCommand(context, context.commands[iCommand]);
            }
        }

        // Getting here means we couldn't find the aliased command. Should not get here.
        return false;
    }
}

bool vkbBuildIsInstanceLevelCommand(vkbBuild &context, vkbBuildCommand &command)
{
    if (command.alias == "") {
        if (command.parameters.size() > 0) {
            if (command.parameters[0].type == "VkInstance" || vkbBuildIsTypeChildOf(context, "VkInstance", command.parameters[0].type)) {
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        // It's aliased. We need to call this based on the aliased command.
        for (size_t iCommand = 0; iCommand < context.commands.size(); ++iCommand) {
            if (context.commands[iCommand].name == command.alias) {
                return vkbBuildIsInstanceLevelCommand(context, context.commands[iCommand]);
            }
        }

        // Getting here means we couldn't find the aliased command. Should not get here.
        return false;
    }
}

vkbResult vkbBuildGenerateCode_C_LoadDeviceAPI(vkbBuild &context, std::string &codeOut)
{
    // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
    std::vector<std::string> outputCommands;
    outputCommands.push_back("vkGetDeviceProcAddr");  // <-- Don't do vkGetInstanceProcAddr here. This is done in the template in a special way.

    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];
        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                size_t iCommand;
                if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, requireCommand.name) && vkbBuildIsDeviceLevelCommand(context, context.commands[iCommand])) {
                        if (outputCommands.size() > 1) {
                            codeOut += "\n    ";
                        }
                        codeOut += "pAPI->" + context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")pAPI->vkGetDeviceProcAddr(device, \"" + context.commands[iCommand].name + "\");";
                        outputCommands.push_back(requireCommand.name);
                    }
                }
            }
        }
    }

    // Platform-independent extensions.
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        vkbBuildExtension &extension = context.extensions[iExtension];
        if (extension.platform == "") {
            for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                vkbBuildRequire &require = extension.requires[iRequire];
                for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                    vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                    size_t iCommand;
                    if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                        if (!vkbContains(outputCommands, requireCommand.name) && vkbBuildIsDeviceLevelCommand(context, context.commands[iCommand])) {
                            if (outputCommands.size() > 1) {
                                codeOut += "\n    ";
                            }
                            codeOut += "pAPI->" + context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")pAPI->vkGetDeviceProcAddr(device, \"" + context.commands[iCommand].name + "\");";
                            outputCommands.push_back(requireCommand.name);
                        }
                    }
                }
            }
        }
    }

    // Platform-specific extensions.
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        vkbBuildPlatform &platform = context.platforms[iPlatform];
        codeOut += "\n#ifdef " + platform.protect;
        {
            for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
                vkbBuildExtension &extension = context.extensions[iExtension];
                if (extension.platform == platform.name) {
                    for (size_t iRequire = 0; iRequire < extension.requires.size(); ++iRequire) {
                        vkbBuildRequire &require = extension.requires[iRequire];
                        for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                            vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                            size_t iCommand;
                            if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                                if (!vkbContains(outputCommands, requireCommand.name) && vkbBuildIsDeviceLevelCommand(context, context.commands[iCommand])) {
                                    if (outputCommands.size() > 1) {
                                        codeOut += "\n    ";
                                    }
                                    codeOut += "pAPI->" + context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")pAPI->vkGetDeviceProcAddr(device, \"" + context.commands[iCommand].name + "\");";
                                    outputCommands.push_back(requireCommand.name);
                                }
                            }
                        }
                    }
                }
            }
        }
        codeOut += "\n#endif /*" + platform.protect + "*/";
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_LoadSafeGlobalAPI(vkbBuild &context, std::string &codeOut)
{
    // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
    std::vector<std::string> outputCommands;
    outputCommands.push_back("vkGetInstanceProcAddr");  // <-- Don't do vkGetInstanceProcAddr here. This is done in the template in a special way.

    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];
        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                size_t iCommand;
                if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, requireCommand.name) && !vkbBuildIsInstanceLevelCommand(context, context.commands[iCommand])) {
                        if (outputCommands.size() > 1) {
                            codeOut += "\n    ";
                        }
                        codeOut += context.commands[iCommand].name + " = (PFN_" + context.commands[iCommand].name + ")vkGetInstanceProcAddr(NULL, \"" + context.commands[iCommand].name + "\");";
                        outputCommands.push_back(requireCommand.name);
                    }
                }
            }
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_SafeGlobalAPIDocs(vkbBuild &context, std::string &codeOut)
{
    // Features.
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        vkbBuildFeature &feature = context.features[iFeature];

        // This should be in a nice order. Features first, then platform-independent extensions, then platform-specific extensions.
        std::vector<std::string> outputCommands;
        codeOut += "\nVulkan " + feature.number + "\n";

        // Manually add vkGetInstanceProcAddr for version 1.0.
        if (feature.number == "1.0") {
            codeOut += "    vkGetInstanceProcAddr";
            outputCommands.push_back("vkGetInstanceProcAddr");
        }
        

        for (size_t iRequire = 0; iRequire < feature.requires.size(); ++iRequire) {
            vkbBuildRequire &require = feature.requires[iRequire];
            for (size_t iRequireCommand = 0; iRequireCommand < require.commands.size(); ++iRequireCommand) {
                vkbBuildRequireCommand &requireCommand = require.commands[iRequireCommand];

                size_t iCommand;
                if (vkbBuildFindCommandByName(context, requireCommand.name.c_str(), &iCommand)) {
                    if (!vkbContains(outputCommands, requireCommand.name) && !vkbBuildIsInstanceLevelCommand(context, context.commands[iCommand])) {
                        if (outputCommands.size() == 0) {
                            codeOut += "    ";
                        } else {
                            codeOut += "\n    ";
                        }
                        codeOut += context.commands[iCommand].name;
                        outputCommands.push_back(requireCommand.name);
                    }
                }
            }
        }
    }

    return VKB_SUCCESS;
}



vkbResult vkbBuildGetVulkanVersion(vkbBuild &context, std::string &versionOut)
{
    // The version can be retrieved from the last "feature" section and the value of VK_HEADER_VERSION.
    versionOut = context.features.back().number;

    for (auto type : context.types) {
        if (type.category == "define") {
            if (type.name == "VK_HEADER_VERSION") {
                std::string defineValue = vkbBuildCleanDefineValue(type.verbatimValue);
                const char* src = defineValue.c_str();
                const char* beg = strstr(src, type.name.c_str()) + type.name.length();

                versionOut += "." + vkbTrim(beg);
                break;
            }
        }
    }

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_VulkanVersion(vkbBuild &context, std::string &codeOut)
{
    std::string version;
    vkbResult result = vkbBuildGetVulkanVersion(context, version);
    if (result != VKB_SUCCESS) {
        return result;
    }

    codeOut += version;

    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C_Revision(vkbBuild &context, std::string &codeOut)
{
    // Rules for the revision number:
    // 1) If the Vulkan version has changed, reset the revision to 0, otherwise increment by 1.
    // 2) If vkbind.h cannot be found, set to 0.
    std::string revision;

    size_t fileSize;
    char* pFileData;
    if (vkbOpenAndReadTextFile("../../vkbind.h", &fileSize, &pFileData) == VKB_SUCCESS) {
        // We need to parse the previous version.
        std::string prevVersionMajor;
        std::string prevVersionMinor;
        std::string prevVersionHeader;
        std::string prevVersionRevision;
        const char* versionBeg = strstr((const char*)pFileData, "vkbind - v");
        if (versionBeg != NULL) {
            versionBeg += strlen("vkbind - v");

            std::string currentVulkanVersion;
            vkbResult result = vkbBuildGetVulkanVersion(context, currentVulkanVersion);
            if (result != VKB_SUCCESS) {
                return result;
            }

            const char* cursorBeg = versionBeg;
            const char* cursorEnd = versionBeg;

            cursorEnd = strstr(cursorBeg, ".");
            prevVersionMajor = std::string(cursorBeg, cursorEnd-cursorBeg);
            cursorBeg = cursorEnd + 1;

            cursorEnd = strstr(cursorBeg, ".");
            prevVersionMinor = std::string(cursorBeg, cursorEnd-cursorBeg);
            cursorBeg = cursorEnd + 1;

            cursorEnd = strstr(cursorBeg, ".");
            prevVersionHeader = std::string(cursorBeg, cursorEnd-cursorBeg);
            cursorBeg = cursorEnd + 1;

            cursorEnd = strstr(cursorBeg, " ");
            prevVersionRevision = std::string(cursorBeg, cursorEnd-cursorBeg);
            cursorBeg = cursorEnd + 1;

            std::string previousVulkanVersion = prevVersionMajor + "." + prevVersionMinor + "." + prevVersionHeader;
            if (currentVulkanVersion == previousVulkanVersion) {
                // Vulkan versions are the same, so increment.
                revision = std::to_string(atoi(prevVersionRevision.c_str()) + 1);
            } else {
                // Vulkan versions are different, so reset.
                revision = "0";
            }
        } else {
            // Couldn't find the previous version.
            revision = "0";
        }
    } else {
        // vkbind.h was not found.
        revision = "0";
    }

    codeOut += revision;
    return VKB_SUCCESS;
}

#include <time.h>
vkbResult vkbBuildGenerateCode_C_Date(vkbBuild &context, std::string &codeOut)
{
    (void)context;

    time_t t = time(NULL);

    char dateStr[32];
#if defined(_MSC_VER)
    struct tm local;
    localtime_s(&local, &t);
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &local);
#else
    struct tm *local = localtime(&t);
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", local);
#endif

    codeOut += dateStr;
    return VKB_SUCCESS;
}

vkbResult vkbBuildGenerateCode_C(vkbBuild &context, const char* tag, std::string &codeOut)
{
    if (tag == NULL) {
        return VKB_INVALID_ARGS;
    }

    vkbResult result = VKB_INVALID_ARGS;
    if (strcmp(tag, "/*<<vulkan_main>>*/") == 0) {
        result = vkbBuildGenerateCode_C_Main(context, codeOut);
    }
    if (strcmp(tag, "/*<<vulkan_funcpointers_decl_global>>*/") == 0) {
        result = vkbBuildGenerateCode_C_FuncPointersDeclGlobal(context, 0, codeOut);
    }
    if (strcmp(tag, "/*<<vulkan_funcpointers_decl_global:4>>*/") == 0) {
        result = vkbBuildGenerateCode_C_FuncPointersDeclGlobal(context, 4, codeOut);
    }
    if (strcmp(tag, "/*<<load_global_api_funcpointers>>*/") == 0) {
        result = vkbBuildGenerateCode_C_LoadGlobalAPIFuncPointers(context, codeOut);
    }
    if (strcmp(tag, "/*<<set_struct_api_from_global>>*/") == 0) {
        result = vkbBuildGenerateCode_C_SetStructAPIFromGlobal(context, codeOut);
    }
    if (strcmp(tag, "/*<<set_global_api_from_struct>>*/") == 0) {
        result = vkbBuildGenerateCode_C_SetGlobalAPIFromStruct(context, codeOut);
    }
    if (strcmp(tag, "/*<<load_instance_api>>*/") == 0) {
        result = vkbBuildGenerateCode_C_LoadInstanceAPI(context, codeOut);
    }
    if (strcmp(tag, "/*<<load_device_api>>*/") == 0) {
        result = vkbBuildGenerateCode_C_LoadDeviceAPI(context, codeOut);
    }
    if (strcmp(tag, "/*<<load_safe_global_api>>*/") == 0) {
        result = vkbBuildGenerateCode_C_LoadSafeGlobalAPI(context, codeOut);
    }
    if (strcmp(tag, "<<safe_global_api_docs>>") == 0) {
        result = vkbBuildGenerateCode_C_SafeGlobalAPIDocs(context, codeOut);
    }
    if (strcmp(tag, "<<vulkan_version>>") == 0) {
        result = vkbBuildGenerateCode_C_VulkanVersion(context, codeOut);
    }
    if (strcmp(tag, "<<revision>>") == 0) {
        result = vkbBuildGenerateCode_C_Revision(context, codeOut);
    }
    if (strcmp(tag, "<<date>>") == 0) {
        result = vkbBuildGenerateCode_C_Date(context, codeOut);
    }

    return result;
}

vkbResult vkbBuildGenerateLib_C(vkbBuild &context, const char* outputFilePath)
{
    if (outputFilePath == NULL) {
        return VKB_INVALID_ARGS;
    }

    // Before doing anything we need to grab the template.
    size_t templateFileSize;
    char* pTemplateFileData;
    vkbResult result = vkbOpenAndReadTextFile(VKB_BUILD_TEMPLATE_PATH, &templateFileSize, &pTemplateFileData);
    if (result != VKB_SUCCESS) {
        return result;
    }

    std::string outputStr = pTemplateFileData;
    free(pTemplateFileData);

    // There will be a series of tags that we need to replace with generated code.
    const char* tags[] = {
        "/*<<vulkan_main>>*/",
        "/*<<vulkan_funcpointers_decl_global>>*/",
        "/*<<vulkan_funcpointers_decl_global:4>>*/",
        "/*<<load_global_api_funcpointers>>*/",
        "/*<<set_struct_api_from_global>>*/",
        "/*<<set_global_api_from_struct>>*/",
        "/*<<load_instance_api>>*/",
        "/*<<load_device_api>>*/",
        "/*<<load_safe_global_api>>*/",
        "<<safe_global_api_docs>>",
        "<<vulkan_version>>",
        "<<revision>>",
        "<<date>>",
    };

    for (size_t iTag = 0; iTag < sizeof(tags)/sizeof(tags[0]); ++iTag) {
        std::string generatedCode;
        result = vkbBuildGenerateCode_C(context, tags[iTag], generatedCode);
        if (result != VKB_SUCCESS) {
            return result;
        }

        vkbReplaceAllInline(outputStr, tags[iTag], generatedCode);
    }

    vkbOpenAndWriteTextFile(outputFilePath, outputStr.c_str());
    return VKB_SUCCESS;
}

#include <io.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    bool forceDownload = true;
    if (forceDownload || _access_s(VKB_BUILD_XML_PATH, 04) != 0) {   // 04 = Read access.
        printf("vk.xml not found. Attempting to download...\n");
        std::string cmd = "curl -o " VKB_BUILD_XML_PATH " https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/xml/vk.xml";
        int result = system(cmd.c_str());
        if (result != 0) {
            printf("Failed to download vk.xml\n");
            return result;
        }
    }

    tinyxml2::XMLError xmlError;

    tinyxml2::XMLDocument doc;
    xmlError = doc.LoadFile(VKB_BUILD_XML_PATH);
    if (xmlError != tinyxml2::XML_SUCCESS) {
        printf("Failed to parse vk.xml\n");
        return (int)xmlError;
    }

    // The root node is the <registry> node.
    tinyxml2::XMLElement* pRoot = doc.RootElement();
    if (pRoot == NULL) {
        printf("Failed to retrieve root node.\n");
        return -1;
    }

    if (strcmp(pRoot->Name(), "registry") != 0) {
        printf("Unexpected root node. Expecting \"registry\", but got \"%s\"", pRoot->Name());
        return -1;
    }


    vkbBuild context;

    // Enumerate over each child of the root node.
    for (tinyxml2::XMLNode* pChild = pRoot->FirstChild(); pChild != NULL; pChild = pChild->NextSibling()) {
        tinyxml2::XMLElement* pChildElement = pChild->ToElement();
        assert(pChildElement != NULL);

        if (strcmp(pChildElement->Name(), "platforms") == 0) {
            vkbBuildParsePlatforms(context, pChildElement);
        }
        if (strcmp(pChildElement->Name(), "tags") == 0) {
            vkbBuildParseTags(context, pChildElement);
        }
        if (strcmp(pChildElement->Name(), "types") == 0) {
            vkbBuildParseTypes(context, pChildElement);
        }
        if (strcmp(pChildElement->Name(), "enums") == 0) {
            vkbBuildParseEnums(context, pChildElement);
        }
        if (strcmp(pChildElement->Name(), "commands") == 0) {
            vkbBuildParseCommands(context, pChildElement);
        }
        if (strcmp(pChildElement->Name(), "feature") == 0) {
            vkbBuildParseFeature(context, pChildElement);
        }
        if (strcmp(pChildElement->Name(), "extensions") == 0) {
            vkbBuildParseExtensions(context, pChildElement);
        }

        //printf("NODE (%d): %s\n", pChild->GetLineNum(), pChild->ToElement()->Name());
    }

#if 1
    // Platforms.
    printf("=== PLATFORMS ===\n");
    for (size_t iPlatform = 0; iPlatform < context.platforms.size(); ++iPlatform) {
        printf("%s: %s\n", context.platforms[iPlatform].name.c_str(), context.platforms[iPlatform].protect.c_str());
    }

    // Types.
    printf("=== TYPES ===\n");
    for (size_t iType = 0; iType < context.types.size(); ++iType) {
        printf("%s %s\n", context.types[iType].category.c_str(), context.types[iType].name.c_str());
    }

    // Commands.
    printf("=== COMMANDS ===\n");
    for (size_t iCommand = 0; iCommand < context.commands.size(); ++iCommand) {
        printf("%s\n", context.commands[iCommand].name.c_str());
    }

    // Features.
    printf("=== FEATURES ===\n");
    for (size_t iFeature = 0; iFeature < context.features.size(); ++iFeature) {
        printf("%s\n", context.features[iFeature].name.c_str());
    }

    // Extensions.
    printf("=== EXTENSION ===\n");
    for (size_t iExtension = 0; iExtension < context.extensions.size(); ++iExtension) {
        printf("%s\n", context.extensions[iExtension].name.c_str());
    }
#endif

    vkbResult result = vkbBuildGenerateLib_C(context, "../../vkbind.h");
    if (result != VKB_SUCCESS) {
        printf("Failed to generate C code.\n");
        return result;
    }

    return 0;
}