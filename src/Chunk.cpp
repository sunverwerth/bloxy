#include "Chunk.h"
#include "ChunkGenerator.h"
#include "Model.h"
#include "GLMesh.h"

#include <sstream>
#include <fstream>
#include <iomanip>

BlockType getBlockAt(int x, int y, int z);

Chunk::Chunk(int x, int y, int z) : gridx(x), gridy(y), gridz(z) {
}

Chunk::~Chunk() {
	if (blocks) {
		delete[] blocks;
	}
	if (model) {
		delete model;
	}
	if (waterModel) {
		delete waterModel;
	}
}

Chunk* getChunk(int gridx, int gridy, int gridz);

void Chunk::generateBlocks(ChunkGenerator* gen) {
	gen->init(gridx*chunkSize, gridz*chunkSize);
	if (blocks) {
		delete[] blocks;
	}
	blocks = new BlockType[chunkSize*chunkSize*chunkSize];
	for (int y = 0; y < chunkSize; ++y) {
		for (int z = 0; z < chunkSize; ++z) {
			for (int x = 0; x < chunkSize; ++x) {
				auto block = gen->getBlockAt(gridx*chunkSize + x, gridy*chunkSize + y, gridz*chunkSize + z);
				if (block == BlockType::WOOD) {
					liveBlocks.push_back(DynamicBlock{ x, y, z, 10 });
				}
				blocks[y * chunkSize*chunkSize + z * chunkSize + x] = block;
			}
		}
	}
	
	/*std::stringstream sstr;
	sstr << std::setfill('0') << std::setw(8) << std::hex << gridx;
	sstr << std::setfill('0') << std::setw(8) << std::hex << gridy;
	sstr << std::setfill('0') << std::setw(8) << std::hex << gridz;
	std::string filename = sstr.str();
	std::string name2(".");
	for (int i = 0; i < filename.size() / 2; ++i) {
		name2 += "/" + filename.substr(i * 2, 2);
	}
	name2 = filename + ".chunk";
	auto file = std::ofstream(name2.c_str());
	if (file.is_open()) {
		file.write((const char*)blocks, sizeof(BlockType) * chunkSize*chunkSize*chunkSize);
	}*/
}

bool isSolid(BlockType block, bool isWater) {
	return isWater ? block != BlockType::AIR : block != BlockType::AIR && block != BlockType::WATER;
}

BlockType Chunk::getBlockAt(int x, int y, int z) {
	if (x < 0 || y < 0 || z < 0) {
		return BlockType::AIR;
	}
	if (x > chunkSize - 1 || y > chunkSize - 1 || z > chunkSize - 1)
	{
		return BlockType::AIR;
	}
	return blocks[y * chunkSize*chunkSize + z * chunkSize + x];
}

void Chunk::setBlockAt(int x, int y, int z, BlockType type) {
	if (getBlockAt(x, y, z) == type) return;
	blocks[y * chunkSize*chunkSize + z * chunkSize + x] = type;
	isDirty = true;
	if (x == 0) {
		auto chunk = getChunk(gridx - 1, gridy, gridz);
		if (chunk) chunk->isDirty = true;
	}
	if (y == 0) {
		auto chunk = getChunk(gridx, gridy - 1, gridz);
		if (chunk) chunk->isDirty = true;
	}
	if (z == 0) {
		auto chunk = getChunk(gridx, gridy, gridz - 1);
		if (chunk) chunk->isDirty = true;
	}
	if (x == chunkSize - 1) {
		auto chunk = getChunk(gridx + 1, gridy, gridz);
		if (chunk) chunk->isDirty = true;
	}
	if (y == chunkSize - 1) {
		auto chunk = getChunk(gridx, gridy + 1, gridz);
		if (chunk) chunk->isDirty = true;
	}
	if (z == chunkSize - 1) {
		auto chunk = getChunk(gridx, gridy, gridz + 1);
		if (chunk) chunk->isDirty = true;
	}
}

Vector4 Chunk::calcLight(int x, int y, int z) {
	int num = 0;
	int sx = gridx*chunkSize;
	int sy = gridy*chunkSize;
	int sz = gridz*chunkSize;
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			for (int k = 0; k < 2; ++k) {
				if (isSolid(::getBlockAt(sx + x + i, sy + y + j, sz + z + k), false)) {
					++num;
				}
			}
		}
	}
	float l = 1.0f - (float)num / 8;
	return Vector4(l, l, l, 1);
}

void Chunk::generateModel() {
	isDirty = false;
	if (!model) {
		model = new Model();
		model->position = Vector3(gridx*chunkSize, gridy*chunkSize, gridz*chunkSize);
		model->rotation = Quaternion::identity;
		model->material = chunkMaterial;
		model->mesh = new GLMesh({
			{ 3, GL_FLOAT, sizeof(float)},
			{ 2, GL_FLOAT, sizeof(float) },
			{ 3, GL_FLOAT, sizeof(float) },
			{ 4, GL_FLOAT, sizeof(float) },
		});
	}

	if (!waterModel) {
		waterModel = new Model();
		waterModel->position = Vector3(gridx*chunkSize, gridy*chunkSize, gridz*chunkSize);
		waterModel->rotation = Quaternion::identity;
		waterModel->material = waterMaterial;
		waterModel->mesh = new GLMesh({
			{ 3, GL_FLOAT, sizeof(float) },
			{ 2, GL_FLOAT, sizeof(float) },
			{ 3, GL_FLOAT, sizeof(float) },
			{ 4, GL_FLOAT, sizeof(float) },
		});
	}

	std::vector<Vertex> chunkVertices;
	std::vector<Vertex> waterVertices;

	auto leftChunk = getChunk(gridx - 1, gridy, gridz);
	auto rightChunk = getChunk(gridx + 1, gridy, gridz);
	auto topChunk = getChunk(gridx, gridy + 1, gridz);
	auto bottomChunk = getChunk(gridx, gridy - 1, gridz);
	auto frontChunk = getChunk(gridx, gridy, gridz + 1);
	auto backChunk = getChunk(gridx, gridy, gridz - 1);

	for (int y = 0; y < chunkSize; ++y) {
		for (int z = 0; z < chunkSize; ++z) {
			for (int x = 0; x < chunkSize; ++x) {
				auto block = getBlockAt(x, y, z);
				bool isWater = block == BlockType::WATER;
				if (block == BlockType::AIR) {
					continue;
				}

				bool renderBottom = y == 0 ? (bottomChunk ? !isSolid(bottomChunk->getBlockAt(x, chunkSize - 1, z), isWater) : false) : !isSolid(getBlockAt(x, y - 1, z), isWater);
				bool renderTop = y == chunkSize - 1 ? (topChunk ? !isSolid(topChunk->getBlockAt(x, 0, z), isWater) : false) : !isSolid(getBlockAt(x, y + 1, z), isWater);

				bool renderLeft = x == 0 ? (leftChunk ? !isSolid(leftChunk->getBlockAt(chunkSize - 1, y, z), isWater) : false) : !isSolid(getBlockAt(x - 1, y, z), isWater);
				bool renderRight = x == chunkSize - 1 ? (rightChunk ? !isSolid(rightChunk->getBlockAt(0, y, z), isWater) : false) : !isSolid(getBlockAt(x + 1, y, z), isWater);

				bool renderBack = z == 0 ? (backChunk ? !isSolid(backChunk->getBlockAt(x, y, chunkSize - 1), isWater) : false) : !isSolid(getBlockAt(x, y, z - 1), isWater);
				bool renderFront = z == chunkSize - 1 ? (frontChunk ? !isSolid(frontChunk->getBlockAt(x, y, 0), isWater) : false) : !isSolid(getBlockAt(x, y, z + 1), isWater);

				if (!(renderTop || renderBottom || renderFront || renderBack || renderLeft || renderRight)) continue;

				auto uv = Vector2(1.0f / 16 * ((int)block % 16), 1.0f - 1.0f / 16 * (1 + (int)block / 16));
				auto uvtop(uv);

				auto& vertices = isWater ? waterVertices : chunkVertices;

				float h = isWater && renderTop ? 0.9f : 1.0f;
				Vector4 light(1, 1, 1, 1);
				Vector4 dark(0.5, 0.5, 0.5, 1);

				Vector4 color = block == BlockType::LEAVES ? Vector4(0, 0.8, 0, 1) : Vector4::white;

				// top
				if (renderTop) {
					if (block == BlockType::GRASS) uvtop = Vector2(0, 1.0f - 1.0f / 16);

					vertices.push_back({ Vector3(x, y + h, z), uvtop + Vector2(0,0), Vector3::up, color * calcLight(x - 1 ,y + 1,z - 1) });
					vertices.push_back({ Vector3(x, y + h, z + 1), uvtop + Vector2(0,1.0f / 16), Vector3::up,  color * calcLight(x - 1,y + 1,z) });
					vertices.push_back({ Vector3(x + 1, y + h, z + 1), uvtop + Vector2(1.0f / 16,1.0f / 16), Vector3::up,  color * calcLight(x,y + 1,z) });
					vertices.push_back({ Vector3(x + 1, y + h, z), uvtop + Vector2(1.0f / 16,0), Vector3::up,  color * calcLight(x,y + 1,z - 1) });
				}

				// bottom
				if (renderBottom) {
					if (block == BlockType::GRASS) uvtop = Vector2(2.0f / 16, 1.0f - 1.0f / 16);
					vertices.push_back({ Vector3(x + 1, y, z), uvtop + Vector2(1.0f / 16,0), Vector3::down, color * calcLight(x ,y - 2,z - 1) });
					vertices.push_back({ Vector3(x + 1, y, z + 1), uvtop + Vector2(1.0f / 16,1.0f / 16), Vector3::down, color * calcLight(x,y - 2,z) });
					vertices.push_back({ Vector3(x, y, z + 1), uvtop + Vector2(0,1.0f / 16), Vector3::down, color * calcLight(x - 1,y - 2,z) });
					vertices.push_back({ Vector3(x, y, z), uvtop + Vector2(0,0), Vector3::down, color * calcLight(x - 1,y - 2,z - 1) });
				}

				// front
				if (renderFront) {
					vertices.push_back({ Vector3(x, y, z + 1), uv + Vector2(0,0), Vector3::backward, color * calcLight(x - 1,y - 1,z + 1) });
					vertices.push_back({ Vector3(x + 1, y, z + 1), uv + Vector2(1.0f / 16,0), Vector3::backward, color * calcLight(x,y - 1,z + 1) });
					vertices.push_back({ Vector3(x + 1, y + h, z + 1), uv + Vector2(1.0f / 16,1.0f / 16), Vector3::backward, color * calcLight(x,y,z + 1) });
					vertices.push_back({ Vector3(x, y + h, z + 1), uv + Vector2(0, 1.0f / 16), Vector3::backward, color * calcLight(x - 1,y,z + 1) });
				}

				// back
				if (renderBack) {
					vertices.push_back({ Vector3(x, y + h, z), uv + Vector2(0, 1.0f / 16), Vector3::forward, color * calcLight(x - 1,y,z - 2) });
					vertices.push_back({ Vector3(x + 1, y + h, z), uv + Vector2(1.0f / 16,1.0f / 16), Vector3::forward, color * calcLight(x,y,z - 2) });
					vertices.push_back({ Vector3(x + 1, y, z), uv + Vector2(1.0f / 16,0), Vector3::forward, color * calcLight(x ,y - 1,z - 2) });
					vertices.push_back({ Vector3(x, y, z), uv + Vector2(0,0), Vector3::forward, color * calcLight(x - 1,y - 1,z - 2) });
				}

				// right
				if (renderRight) {
					vertices.push_back({ Vector3(x + 1, y, z), uv + Vector2(0,0), Vector3::right, color * calcLight(x + 1,y - 1,z - 1) });
					vertices.push_back({ Vector3(x + 1, y + h, z), uv + Vector2(0,1.0f / 16), Vector3::right, color * calcLight(x + 1,y,z - 1) });
					vertices.push_back({ Vector3(x + 1, y + h, z + 1), uv + Vector2(1.0f / 16,1.0f / 16), Vector3::right, color * calcLight(x + 1,y,z) });
					vertices.push_back({ Vector3(x + 1, y, z + 1), uv + Vector2(1.0f / 16,0), Vector3::right, color * calcLight(x + 1,y - 1,z) });
				}

				// left
				if (renderLeft) {
					vertices.push_back({ Vector3(x, y, z + 1), uv + Vector2(0, 0), Vector3::left, color * calcLight(x - 2,y - 1,z) });
					vertices.push_back({ Vector3(x, y + h, z + 1), uv + Vector2(0, 1.0f / 16), Vector3::left, color * calcLight(x - 2,y ,z) });
					vertices.push_back({ Vector3(x, y + h, z), uv + Vector2(1.0f / 16, 1.0f / 16), Vector3::left, color * calcLight(x - 2,y ,z - 1) });
					vertices.push_back({ Vector3(x, y, z), uv + Vector2(1.0f / 16,0), Vector3::left, color * calcLight(x - 2,y - 1,z - 1) });
				}
			}
		}
	}

	model->mesh->setVertices(sizeof(Vertex)*chunkVertices.size(), chunkVertices.data(), GL_STATIC_DRAW);

	std::vector<unsigned int> chunkIndices;
	for (int i = 0; i < chunkVertices.size() / 4; ++i) {
		chunkIndices.push_back(i * 4 + 0);
		chunkIndices.push_back(i * 4 + 1);
		chunkIndices.push_back(i * 4 + 2);
		chunkIndices.push_back(i * 4 + 2);
		chunkIndices.push_back(i * 4 + 3);
		chunkIndices.push_back(i * 4 + 0);
	}
	model->mesh->setIndices(sizeof(unsigned int)*chunkIndices.size(), chunkIndices.data(), GL_STATIC_DRAW);

	waterModel->mesh->setVertices(sizeof(Vertex)*waterVertices.size(), waterVertices.data(), GL_STATIC_DRAW);

	std::vector<unsigned int> waterIndices;
	for (int i = 0; i < waterVertices.size() / 4; ++i) {
		waterIndices.push_back(i * 4 + 0);
		waterIndices.push_back(i * 4 + 1);
		waterIndices.push_back(i * 4 + 2);
		waterIndices.push_back(i * 4 + 2);
		waterIndices.push_back(i * 4 + 3);
		waterIndices.push_back(i * 4 + 0);
	}
	waterModel->mesh->setIndices(sizeof(unsigned int)*waterIndices.size(), waterIndices.data(), GL_STATIC_DRAW);
}

Material* Chunk::chunkMaterial = nullptr;
Material* Chunk::waterMaterial = nullptr;