#ifndef __ASSEMBLYDATA_HXX__
#define __ASSEMBLYDATA_HXX__
#include <cinttypes>
#include <vector>

#include "AssemblyReader.hxx"
#include "Property.hxx"
#include "CLIMetadataTableRows.hxx"

class AssemblyData
{
public:
    // Image header
    ImageFileHeader fileHeader;

    // Sections header
    std::vector<ImageSectionHeader> sections;

    // CLR header
    CLIHeader cliHeader;

    struct CLIMetaData {
        uint32_t cliMetadataOffset; // In memory only

        std::vector<uint16_t> version;
        uint16_t streamsCount = 0;

        struct CLIStream {
            uint32_t offset = 0;
            uint32_t size = 0;
            std::vector<uint8_t> name;
        };

        std::vector<CLIStream> streams;
        uint32_t getStreamOffset(const std::vector<uint8_t>& name) const;
    } cliMetadata;

    struct CLIMetaDataTables {
        ModuleRow module;
        std::vector<TypeRefRow> _TypeRef;
        std::vector<TypeDefRow> _TypeDef;
        std::vector<FieldRow> _Field;
        std::vector<MethodDefRow> _MethodDef;
        std::vector<ParamRow> _Param;
        std::vector<InterfaceImplRow> _InterfaceImpl;
        std::vector<MemberRefRow> _MemberRef;
        std::vector<ConstantRow> _Constant;
        std::vector<CustomAttributeRow> _CustomAttribute;
        std::vector<FieldMarshalRow> _FieldMarshal;
        std::vector<DeclSecurityRow> _DeclSecurity;
        std::vector<ClassLayoutRow> _ClassLayout;
        std::vector<FieldLayoutRow> _FieldLayout;
        std::vector<std::vector<uint32_t> > _StandAloneSig;
        std::vector<EventMapRow> _EventMap;
        std::vector<EventRow> _Event;
        std::vector<PropertyMapRow> _PropertyMap;
        std::vector<PropertyRow> _Property;
        std::vector<MethodSemanticsRow> _MethodSemantics;
        std::vector<MethodImplRow> _MethodImpl;
        std::vector<std::vector<uint16_t> > _ModuleRef;
        std::vector<std::vector<uint32_t> > _TypeSpec;
        std::vector<ImplMapRow> _ImplMap;
        std::vector<FieldRVARow> _FieldRVA;
        std::vector<AssemblyRow> _Assembly;
        std::vector<uint32_t> _AssemblyProcessor;
        std::vector<AssemblyOSRow> _AssemblyOS;
        std::vector<AssemblyRefRow> _AssemblyRef;
        std::vector<AssemblyRefProcessorRow> _AssemblyRefProcessor;
        std::vector<AssemblyRefOSRow> _AssemblyRefOS;
        std::vector<FileRow> _File;
        std::vector<ExportedTypeRow> _ExportedType;
        std::vector<ManifestResourceRow> _ManifestResource;
        std::vector<NestedClassRow> _NestedClass;
        std::vector<GenericParamRow> _GenericParam;
        std::vector<MethodSpecRow> _MethodSpec;
        std::vector<GenericParamConstraintRow> _GenericParamConstraint;
    } cliMetaDataTables;

    AssemblyData(std::string strFilePathName);
    AssemblyData(const std::vector<uint8_t>& assembly_bytes);
    uint32_t getDataOffset(uint32_t address) const;
    void getMethodBody(uint32_t index, MethodBody& methodBody);

private:
    AssemblyReader reader;
    void InitAssembly(); // called from constructor
    void FillTables();
};

#endif
