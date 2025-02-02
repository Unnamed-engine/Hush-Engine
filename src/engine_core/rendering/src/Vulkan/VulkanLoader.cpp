#include "VulkanLoader.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include "VkTypes.hpp"
#include "VulkanRenderer.hpp"

std::optional<std::vector<std::shared_ptr<Hush::MeshAsset>>> Hush::VulkanLoader::LoadGltfMeshes(VulkanRenderer* engine, std::filesystem::path filePath)
{
	if (!std::filesystem::exists(filePath))
    {
        LogFormat(ELogLevel::Error, "Failed to load mesh at {} file does not exist", filePath.string());
        return std::nullopt;
    }
    fastgltf::Expected<fastgltf::GltfDataBuffer> data_result = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (!data_result)
    {
        LogFormat(ELogLevel::Error, "Failed to load mesh at {} error: {}", filePath.string(),
                  fastgltf::getErrorMessage(data_result.error()));
        return std::nullopt;
    }

    fastgltf::GltfDataBuffer &data = data_result.get();

	constexpr fastgltf::Options loadingOptions = fastgltf::Options::LoadExternalBuffers;

	fastgltf::Parser parser{};

	fastgltf::Expected<fastgltf::Asset> loadedAsset = parser.loadGltfBinary(data, filePath.parent_path(), loadingOptions);

	HUSH_ASSERT(loadedAsset, "GLTF asset at {} not properly loaded, error: {}!", filePath.string(), fastgltf::getErrorMessage(loadedAsset.error()));
	
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	std::vector<std::shared_ptr<MeshAsset>> meshes;
	for (const fastgltf::Mesh& mesh : loadedAsset->meshes)
	{
		MeshAsset meshToAdd = CreateMeshAssetFromGltfMesh(mesh, loadedAsset.get(), indices, vertices, engine);
		meshes.push_back(std::make_shared<MeshAsset>(meshToAdd));
	}
	return meshes;
}

Hush::MeshAsset Hush::VulkanLoader::CreateMeshAssetFromGltfMesh(const fastgltf::Mesh &mesh,
                                                                const fastgltf::Asset &asset,
                                                                std::vector<uint32_t> &indicesRef,
                                                                std::vector<Vertex> &verticesRef,
                                                                VulkanRenderer *engine)
{
    MeshAsset resultMesh;

    resultMesh.name = mesh.name;
    // Clear out the vector buffers
    indicesRef.clear();
    verticesRef.clear();

    for (const fastgltf::Primitive &primitive : mesh.primitives)
    {
        size_t initialVertex = verticesRef.size();
        GeoSurface surfaceToAdd{};
        surfaceToAdd.startIndex = static_cast<uint32_t>(indicesRef.size());
        const fastgltf::Accessor &primitiveIdxAccessor = asset.accessors[primitive.indicesAccessor.value()];
        surfaceToAdd.count = static_cast<uint32_t>(primitiveIdxAccessor.count);

        // TODO: Make a function that can load any arbitrary primitive from glTF

        // load indexes
        indicesRef.reserve(indicesRef.size() + primitiveIdxAccessor.count);
        fastgltf::iterateAccessor<uint32_t>(asset, primitiveIdxAccessor, [&](uint32_t idx) {
            indicesRef.push_back(idx + static_cast<uint32_t>(initialVertex));
        });

        std::vector<glm::vec3> vertexBuffer = FindAttributeByName<glm::vec3>(primitive, asset, "POSITION");
        for (const glm::vec3 &v : vertexBuffer)
        {
            Vertex vertexToAdd{};
            vertexToAdd.position = v;
            verticesRef.push_back(vertexToAdd);
        }

        std::vector<glm::vec3> normalBuffer = FindAttributeByName<glm::vec3>(primitive, asset, "NORMAL");
        for (uint32_t i = 0; i < normalBuffer.size(); i++)
        {
            verticesRef.at(i + initialVertex).normal = normalBuffer.at(i);
        }

        // load UVs
        std::vector<glm::vec2> texBuffer = FindAttributeByName<glm::vec2>(primitive, asset, "TEXCOORD_0");

        for (uint32_t i = 0; i < texBuffer.size(); i++)
        {
            verticesRef.at(i + initialVertex).uv_x = texBuffer.at(i).x;
            verticesRef.at(i + initialVertex).uv_y = texBuffer.at(i).y;
        }

        // load vertex colors
        std::vector<glm::vec4> colors = FindAttributeByName<glm::vec4>(primitive, asset, "COLOR_0");

        for (uint32_t i = 0; i < colors.size(); i++)
        {
            verticesRef.at(i + initialVertex).color = colors.at(i);
        }

        resultMesh.surfaces.push_back(surfaceToAdd);
    }
    resultMesh.meshBuffers = engine->UploadMesh(indicesRef, verticesRef);
    return resultMesh;
}

Hush::Result<const uint8_t*, Hush::VulkanLoader::Error> Hush::VulkanLoader::GetDataFromBufferSource(const fastgltf::Buffer& buffer)
{
	const fastgltf::sources::Vector* vectorData = std::get_if<fastgltf::sources::Vector>(&buffer.data);
	if (vectorData != nullptr) {
		return reinterpret_cast<const uint8_t*>(vectorData->bytes.data());
	}
	//Ok, try the ByteView
	const fastgltf::sources::ByteView* byteData = std::get_if<fastgltf::sources::ByteView>(&buffer.data);
	if (byteData != nullptr) {
		return reinterpret_cast<const uint8_t*>(byteData->bytes.data());
	}
	//Else, idk, we don't recognize this yet
	return Error::InvalidMeshFile;
}
