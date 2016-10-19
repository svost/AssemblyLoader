#include <fstream>
#include <iterator>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>
#include <limits>

#include "AssemblyData.hxx"
#include "Formatting.hxx"

using namespace std;

AssemblyData::AssemblyData(string strFilePathName) {
    ifstream AssemblyStream(strFilePathName, ios_base::in | ios_base::binary | ios_base::ate);
    if (!AssemblyStream.is_open()) {
        throw runtime_error("Unable to open assembly file");
    }

    vector<uint8_t> file_bytes;
    file_bytes.reserve(AssemblyStream.tellg());
    AssemblyStream.seekg(ios_base::beg);
    file_bytes.assign((istreambuf_iterator<char>(AssemblyStream)), istreambuf_iterator<char>());
    
    // Check MZ header
	if (file_bytes[0] != 0x4d || file_bytes[1] != 0x5a) {
		throw runtime_error("There is no MZ header");
	}

	// Create reader and continue initialization process
	reader = AssemblyReader(file_bytes);
    InitAssembly();
}

AssemblyData::AssemblyData(const vector<uint8_t>& assembly_bytes) {
	// Check MZ header
	if (assembly_bytes[0] != 0x4d || assembly_bytes[1] != 0x5a) {
		throw runtime_error("There is no MZ header");
	}

	// Create reader and continue initialization process
	reader = AssemblyReader(assembly_bytes);
	InitAssembly();
}

void AssemblyData::InitAssembly() {
    // PE headers
    uint32_t peOffset = reader.read_uint32(0x3c);
	if (reader.read_uint32(peOffset) != 0x4550) {
        // PE header should begin with PE\0\0 magic value.
		throw runtime_error("Invalid PE header");
	}

	uint16_t standard = reader.read_uint16(peOffset + 24);
	uint32_t cliHeaderRVA = 0;

	switch (standard) {
        // CLR has a support for two variations of PE format, PE32 and PE32+.

		case 0x010B: {
			// PE32
			ImageNTHeader32 header;
			reader.read_ntheader32(header, peOffset);
			if (header.optionalHeader.nt.numberOfRvaAndSizes != ImageOptionalDirectoriesNumber32) {
				throw runtime_error("Optional header is invalid.");
			}
			fileHeader = header.fileHeader;
			cliHeaderRVA = header.optionalHeader.nt.directories[ImageDirectoryType::cliHeader].rva;
			peOffset += sizeof(ImageNTHeader32);
		}
		break;
		case 0x020B: {
			// PE32+
			ImageNTHeader64 header;
			reader.read_ntheader64(header, peOffset);
			if (header.optionalHeader.nt.numberOfRvaAndSizes != ImageOptionalDirectoriesNumber64) {
				throw runtime_error("Optional header is invalid.");
			}
			fileHeader = header.fileHeader;
			cliHeaderRVA = header.optionalHeader.nt.directories[ImageDirectoryType::cliHeader].rva;
			peOffset += sizeof(ImageNTHeader64);
		}
		break;
		default:
			throw runtime_error("Unsupported PE format");
	}
	
    // Section Headers
    for (uint32_t i = 0; i < fileHeader.sectionsCount; ++i) {
        ImageSectionHeader header;
        reader.read_sectionheader(header, peOffset);
        sections.push_back(header);
        peOffset += sizeof(ImageSectionHeader);
    }

    // CLI Header 
    {
        uint32_t cliHeaderOffset = getDataOffset(cliHeaderRVA);
        if (cliHeaderOffset == numeric_limits<uint32_t>::max()) {
            throw runtime_error("CLI header could not be found.");
        }

        if (reader.read_uint32(cliHeaderOffset) != sizeof(CLIHeader)) {
            // CLI header size must be identical to one which is defined by CLIHeader structure.
            throw runtime_error("CLI header size is invalid.");
        }

        // Load CLI header
        reader.read_cliheader(cliHeader, cliHeaderOffset);

        if ((cliHeader.flags & CLIHeaderFlags::nativeEntryPoint) != 0) {
            // Reject header if there are any native-specific flags specified.
            throw runtime_error("Only pure IL assemblies are supported.");
        }
    }

    // CLI MetaData section
    cliMetadata.cliMetadataOffset = getDataOffset(cliHeader.metaData.rva);
    if (reader.read_uint32(cliMetadata.cliMetadataOffset) != 0x424A5342) {
        throw runtime_error("Invalid CLI metadata.");
    }

    uint32_t versionLength = reader.read_uint32(cliMetadata.cliMetadataOffset + 12);
    cliMetadata.version.reserve(versionLength);
    reader.read_utf8z(cliMetadata.version, cliMetadata.cliMetadataOffset + 16, versionLength);
    cliMetadata.streamsCount = reader.read_uint16(cliMetadata.cliMetadataOffset + versionLength + 18);

    uint32_t streamsOffset = cliMetadata.cliMetadataOffset + versionLength + 20;
    for (uint32_t i = 0; i < cliMetadata.streamsCount; ++i) {
        CLIMetaData::CLIStream stream = {};
        stream.offset = reader.read_uint32(streamsOffset);
        stream.size = reader.read_uint32(streamsOffset + 4);
        reader.read_asciiz(stream.name, streamsOffset + 8, 32);
        cliMetadata.streams.push_back(stream);
        streamsOffset += 8 + ((stream.name.size() + 4) & 0xFC);
    }

    FillTables();
}

void AssemblyData::FillTables() {
    const auto metaHeaderOffset = cliMetadata.getStreamOffset({'#', '~'});
    const auto stringStreamOffset = cliMetadata.getStreamOffset({'#', 'S', 't', 'r', 'i', 'n', 'g', 's'});
    const auto usStreamOffset = cliMetadata.getStreamOffset({'#', 'U', 'S'});
    const auto guidStreamOffset = cliMetadata.getStreamOffset({'#', 'G', 'U', 'I', 'D'});
	const auto blobStreamOffset = cliMetadata.getStreamOffset({ '#', 'B', 'l', 'o', 'b' });

    const auto heapSizes = reader[metaHeaderOffset + 6];
	const auto valid = reader.read_uint64(metaHeaderOffset + 8);
    const auto sorted = reader.read_uint64(metaHeaderOffset + 16);

	auto& r = reader; // reader can't be captured directly, so make a local reference
    auto readString = [&r, heapSizes, stringStreamOffset](vector<uint16_t>& result) { 
        uint32_t offset = (heapSizes & 0x01) != 0 ? r.read_uint32() : r.read_uint16();
        r.read_utf8z(result, stringStreamOffset + offset, 0xffffffff);
    };
	auto readGuid = [&r, heapSizes, guidStreamOffset](vector<uint8_t>& result) {
        uint32_t index = (heapSizes & 0x02) != 0 ? r.read_uint32() : r.read_uint16();
        if (index != 0) {
            r.read_guid(result, guidStreamOffset + ((index - 1) << 4));
        }
    };
	auto readBlob = [&r, heapSizes, blobStreamOffset](vector<uint8_t>& result) {
        uint32_t index = (heapSizes & 0x04) != 0 ? r.read_uint32() : r.read_uint16();
        uint32_t offset = blobStreamOffset + index;
        uint32_t length;
        ptrdiff_t read = r.read_varsize(length, offset);
        r.read_bytes(result, offset + read, length);
    };

    auto readSignature = [&r, &readBlob](vector<uint32_t>& result) {
        vector<uint8_t> buffer;
        readBlob(buffer);
        uint32_t offset = 0;
        while (offset < buffer.size()) {
            uint32_t value;
            uint32_t read = AssemblyReader::read_varsize(value, buffer, offset);
            result.push_back(value);
            offset += read;
        }
    };

	auto metaDataOffset = metaHeaderOffset + 24;
	reader.seek(metaDataOffset);

    map<CliMetadataTableIndex, uint32_t> mapTableLength;

    for (CliMetadataTableIndex bit = Module; bit <= GenericParamConstraint; ++bit) {
        bool isSet = ((valid >> bit) & 1) != 0;
        if (isSet) {
            // Load table length record for existent and valid table.
            mapTableLength[bit] = reader.read_uint32();
            cout << getTableName(bit) << " " << dec << mapTableLength[bit] << endl;
        } else {
            mapTableLength[bit] = 0;
            cout << getTableName(bit) << " " << dec << mapTableLength[bit] << endl;
        }
    }

    auto readRowIndex = [&r, &mapTableLength](CliMetadataTableIndex tableIndex)->uint32_t {
        if (mapTableLength.find(tableIndex) != mapTableLength.end()) {
            return mapTableLength[tableIndex] >= 0xffff ? r.read_uint32() : r.read_uint16();
        }

        return r.read_uint16();
    };

    auto readRowIndexChoice = [&r, &mapTableLength](const vector<CliMetadataTableIndex>& tables)->pair<uint32_t, CliMetadataTableIndex> {
        uint32_t max = 0;

        for (const auto& tableID : tables) {
            if (mapTableLength.find(tableID) != mapTableLength.end() && max < mapTableLength[tableID]) {
                max = mapTableLength[tableID];
            }
        }

        uint32_t shift = 0, bit = 1;
        while (tables.size() > bit) {
            bit <<= 1;
            ++shift;
        }

        uint32_t index = (max << shift) >= 0xffff ? r.read_uint32() : r.read_uint16();

        return { index, tables[index & (bit - 1)] };
    };

    {
        // Module table
        if (mapTableLength[Module] != 1) {
            throw runtime_error("Module table most contain one and only one row.");
        }
        cliMetaDataTables.module.generation = r.read_uint16();
        readString(cliMetaDataTables.module.name);
        readGuid(cliMetaDataTables.module.guid);

        vector<uint8_t> tmp;
        readGuid(tmp); // encId 
        readGuid(tmp); // endBaseId 
    }

    {
        // TypeRef
        vector<CliMetadataTableIndex> scope = { Module, ModuleRef, AssemblyRef, TypeRef };
        for (uint32_t n = 0; n < mapTableLength[TypeRef]; ++n) {
            TypeRefRow row;
            row.resolutionScope = readRowIndexChoice(scope);
            readString(row.typeName);
            readString(row.typeNamespace);
            cliMetaDataTables._TypeRef.push_back(row);
        }
    }

    {
        // TypeDef
        vector<CliMetadataTableIndex> scope = { TypeDef, TypeRef, TypeSpec };
        for (uint32_t n = 0; n < mapTableLength[TypeDef]; ++n) {
            TypeDefRow row;
            row.flags = reader.read_uint32();
            readString(row.typeName);
            readString(row.typeNamespace);
            row.extendsType = readRowIndexChoice(scope);
            row.fieldList = readRowIndex(Field);
            row.methodList = readRowIndex(MethodDef);
            cliMetaDataTables._TypeDef.push_back(row);
        }
    }

    {
        // Field
        for (uint32_t n = 0; n < mapTableLength[Field]; ++n) {
            FieldRow row;
            row.flags = reader.read_uint16();
            readString(row.name);
            readSignature(row.signature);
            cliMetaDataTables._Field.push_back(row);
        }
    }

    {
        // MethodDef
        for (uint32_t n = 0; n < mapTableLength[MethodDef]; ++n) {
            MethodDefRow row;
            row.rva = reader.read_uint32();
            row.implFlags = reader.read_uint16();
            row.flags = reader.read_uint16();
            readString(row.name);
            readSignature(row.signature);
            row.paramList = readRowIndex(Param);
            cliMetaDataTables._MethodDef.push_back(row);
        }
    }

    {
        // Param
        for (uint32_t n = 0; n < mapTableLength[Param]; ++n) {
            ParamRow row;
            row.flags = reader.read_uint16();
            row.sequence = reader.read_uint16();
            readString(row.name);
            cliMetaDataTables._Param.push_back(row);
        }
    }

}

uint32_t AssemblyData::getDataOffset (uint32_t address) const {
    for (uint32_t n = 0; n < fileHeader.sectionsCount; ++n) {
        if (sections[n].virtualAddress <= address && address < sections[n].virtualAddress + sections[n].virtualSize) {
            return sections[n].pointerToRawData + (address - sections[n].virtualAddress);
        }
    }

    return -1;
}

uint32_t AssemblyData::CLIMetaData::getStreamOffset(const vector<uint8_t>& name) const {
    for (uint32_t i = 0; i < streamsCount; ++i) {
        const vector<uint8_t>& streamName = streams[i].name;
        if (streamName.size() == name.size() && equal(begin(streamName), end(streamName), begin(name))) {
            return cliMetadataOffset + streams[i].offset;
        }
    }

    return -1;
};

