/*
 * Copyright (c) 2012-2018 Daniele Bartolini and individual contributors.
 * License: https://github.com/dbartolini/crown/blob/master/LICENSE
 */

#include "core/containers/array.h"
#include "core/containers/hash_map.h"
#include "core/containers/hash_set.h"
#include "core/filesystem/file.h"
#include "core/filesystem/filesystem.h"
#include "core/filesystem/path.h"
#include "core/filesystem/reader_writer.h"
#include "core/json/json_object.h"
#include "core/json/sjson.h"
#include "core/memory/temp_allocator.h"
#include "core/strings/dynamic_string.h"
#include "core/strings/string_id.h"
#include "resource/compile_options.h"
#include "resource/data_compiler.h"
#include "resource/package_resource.h"
#include "resource/resource_id.h"

namespace crown
{
template<>
struct hash<PackageResource::Resource>
{
	u32 operator()(const PackageResource::Resource& val) const
	{
		return (u32)resource_id(val.type, val.name)._id;
	}
};

namespace package_resource_internal
{
	s32 bring_in_requirements(HashSet<PackageResource::Resource>& output, CompileOptions& opts, ResourceId res_id)
	{
		HashMap<DynamicString, u32> reqs_deffault(default_allocator());
		HashMap<DynamicString, u32>& reqs = hash_map::get(opts._data_compiler._data_requirements, res_id, reqs_deffault);

		auto cur = hash_map::begin(reqs);
		auto end = hash_map::end(reqs);
		for (; cur != end; ++cur)
		{
			HASH_MAP_SKIP_HOLE(reqs, cur);

			const char* req_filename = cur->first.c_str();
			const char* req_type = path::extension(req_filename);
			const u32 req_name_len = u32(req_type - req_filename - 1);

			const StringId64 req_type_hash(req_type);
			const StringId64 req_name_hash(req_filename, req_name_len);
			hash_set::insert(output, PackageResource::Resource(req_type_hash, req_name_hash));

			bring_in_requirements(output, opts, resource_id(req_type_hash, req_name_hash));
		}

		return 0;
	}

	s32 compile_resources(HashSet<PackageResource::Resource>& output, CompileOptions& opts, const char* type, const JsonArray& names)
	{
		const StringId64 type_hash = StringId64(type);

		for (u32 i = 0; i < array::size(names); ++i)
		{
			TempAllocator256 ta;
			DynamicString name(ta);
			sjson::parse_string(names[i], name);
			DATA_COMPILER_ASSERT_RESOURCE_EXISTS(type, name.c_str(), opts);
			name += ".";
			name += type;
			opts.fake_read(name.c_str());

			const StringId64 name_hash = sjson::parse_resource_name(names[i]);
			hash_set::insert(output, PackageResource::Resource(type_hash, name_hash));

			// Bring in requirements
			bring_in_requirements(output, opts, resource_id(type_hash, name_hash));
		}

		return 0;
	}

	s32 compile(CompileOptions& opts)
	{
		TempAllocator4096 ta;
		JsonObject object(ta);
		JsonArray texture(ta);
		JsonArray script(ta);
		JsonArray sound(ta);
		JsonArray mesh(ta);
		JsonArray unit(ta);
		JsonArray sprite(ta);
		JsonArray material(ta);
		JsonArray font(ta);
		JsonArray level(ta);
		JsonArray phyconf(ta);
		JsonArray shader(ta);
		JsonArray sprite_animation(ta);

		Array<PackageResource::Resource> resources(default_allocator());
		HashSet<PackageResource::Resource> resources_set(default_allocator());

		Buffer buf = opts.read();
		sjson::parse(buf, object);

		if (json_object::has(object, "texture"))          sjson::parse_array(object["texture"], texture);
		if (json_object::has(object, "lua"))              sjson::parse_array(object["lua"], script);
		if (json_object::has(object, "sound"))            sjson::parse_array(object["sound"], sound);
		if (json_object::has(object, "mesh"))             sjson::parse_array(object["mesh"], mesh);
		if (json_object::has(object, "unit"))             sjson::parse_array(object["unit"], unit);
		if (json_object::has(object, "sprite"))           sjson::parse_array(object["sprite"], sprite);
		if (json_object::has(object, "material"))         sjson::parse_array(object["material"], material);
		if (json_object::has(object, "font"))             sjson::parse_array(object["font"], font);
		if (json_object::has(object, "level"))            sjson::parse_array(object["level"], level);
		if (json_object::has(object, "physics_config"))   sjson::parse_array(object["physics_config"], phyconf);
		if (json_object::has(object, "shader"))           sjson::parse_array(object["shader"], shader);
		if (json_object::has(object, "sprite_animation")) sjson::parse_array(object["sprite_animation"], sprite_animation);

		if (compile_resources(resources_set, opts, "texture", texture) != 0) return -1;
		if (compile_resources(resources_set, opts, "lua", script) != 0) return -1;
		if (compile_resources(resources_set, opts, "sound", sound) != 0) return -1;
		if (compile_resources(resources_set, opts, "mesh", mesh) != 0) return -1;
		if (compile_resources(resources_set, opts, "unit", unit) != 0) return -1;
		if (compile_resources(resources_set, opts, "sprite", sprite) != 0) return -1;
		if (compile_resources(resources_set, opts, "material", material) != 0) return -1;
		if (compile_resources(resources_set, opts, "font", font) != 0) return -1;
		if (compile_resources(resources_set, opts, "level", level) != 0) return -1;
		if (compile_resources(resources_set, opts, "physics_config", phyconf) != 0) return -1;
		if (compile_resources(resources_set, opts, "shader", shader) != 0) return -1;
		if (compile_resources(resources_set, opts, "sprite_animation", sprite_animation) != 0) return -1;

		// Generate resource list
		auto cur = hash_set::begin(resources_set);
		auto end = hash_set::end(resources_set);
		for (; cur != end; ++cur)
		{
			HASH_SET_SKIP_HOLE(resources_set, cur);

			array::push_back(resources, *cur);
		}

		// Write
		opts.write(RESOURCE_HEADER(RESOURCE_VERSION_PACKAGE));
		opts.write(array::size(resources));

		for (u32 i = 0; i < array::size(resources); ++i)
		{
			opts.write(resources[i].type);
			opts.write(resources[i].name);
		}

		return 0;
	}

	void* load(File& file, Allocator& a)
	{
		BinaryReader br(file);

		u32 version;
		br.read(version);
		CE_ASSERT(version == RESOURCE_HEADER(RESOURCE_VERSION_PACKAGE), "Wrong version");

		u32 num_resources;
		br.read(num_resources);

		PackageResource* pr = CE_NEW(a, PackageResource)(a);
		array::resize(pr->resources, num_resources);
		br.read(array::begin(pr->resources), sizeof(PackageResource::Resource)*num_resources);

		return pr;
	}

	void unload(Allocator& a, void* resource)
	{
		CE_DELETE(a, (PackageResource*)resource);
	}

} // namespace package_resource_internal

} // namespace crown
