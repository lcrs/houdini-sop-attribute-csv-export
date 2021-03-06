#include "SOP_AttributeCSVExport.h"

#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <GEO/GEO_Detail.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <PRM/PRM_SpareData.h>

#define SOP_ATTRIBUTECSVEXPORT_CLASS "class"
#define SOP_ATTRIBUTECSVEXPORT_FILE "file"
#define SOP_ATTRIBUTECSVEXPORT_SKIP_INTRINSIC_ATTRIBUTES "csv_skip_intrinsic"
#define SOP_ATTRIBUTECSVEXPORT_CREATE_OFFSET_ATTRIBUTE "csv_create_offset_attribute"
#define SOP_ATTRIBUTECSVEXPORT_ATTRIB_ORDER "attrib_order"

static PRM_Name s_name_file(SOP_ATTRIBUTECSVEXPORT_FILE, "CSV File");
static PRM_Name s_name_class(SOP_ATTRIBUTECSVEXPORT_CLASS, "Class");
static PRM_Name s_name_skip_intrinsic_attributes(SOP_ATTRIBUTECSVEXPORT_SKIP_INTRINSIC_ATTRIBUTES, "Skip Intrinsic Attributes");
static PRM_Name s_name_create_offset_attribute(SOP_ATTRIBUTECSVEXPORT_CREATE_OFFSET_ATTRIBUTE, "Create Offset Attribute");
static PRM_Name s_name_attrib_order(SOP_ATTRIBUTECSVEXPORT_ATTRIB_ORDER, "Attribute order");

static PRM_Name s_name_class_types[] =
{
    PRM_Name("point", "Point"),
    PRM_Name("vertex", "Vertex"),
    PRM_Name("primitive", "Primitive"),
    PRM_Name("detail", "Detail"),
    PRM_Name(0),
};

static PRM_ChoiceList s_choicelist_class_type(PRM_CHOICELIST_SINGLE, s_name_class_types);
static PRM_Default s_default_skip_intrinsic_attributes(true);
static PRM_Default s_default_create_offset_attribute(true);
static PRM_Default s_default_attrib_order(PRM_STRING, "P Cd");

static PRM_SpareData s_spare_file_picker(PRM_SpareArgs() << PRM_SpareToken(PRM_SpareData::getFileChooserModeToken(),
    PRM_SpareData::getFileChooserModeValRead()) << PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(),
    SOP_AttributeCSVExport::fileExtensionFilterString()));

PRM_Template
SOP_AttributeCSVExport::myTemplateList[] = {
    PRM_Template(PRM_FILE, 1, &s_name_file, 0, 0, 0, 0, &s_spare_file_picker),
    PRM_Template(PRM_ORD, 1, &s_name_class, 0, &s_choicelist_class_type),
    PRM_Template(PRM_TOGGLE, 1, &s_name_skip_intrinsic_attributes, &s_default_skip_intrinsic_attributes),
    PRM_Template(PRM_TOGGLE, 1, &s_name_create_offset_attribute, &s_default_create_offset_attribute),
    PRM_Template(PRM_STRING, 1, &s_name_attrib_order, &s_default_attrib_order),
    PRM_Template()
};


OP_Node*
SOP_AttributeCSVExport::myConstructor(OP_Network* network, const char* name, OP_Operator* op)
{
    return new SOP_AttributeCSVExport(network, name, op);
}


const char*
SOP_AttributeCSVExport::fileExtensionFilterString()
{
    return "*.csv";
}


SOP_AttributeCSVExport::SOP_AttributeCSVExport(OP_Network* network, const char* name, OP_Operator* op) :
    SOP_Node(network, name, op)
{

}


SOP_AttributeCSVExport::~SOP_AttributeCSVExport()
{

}


OP_ERROR
SOP_AttributeCSVExport::cookMySop(OP_Context& context)
{
    fpreal t = context.getTime();
    GA_AttributeOwner attrib_owner = GA_ATTRIB_POINT;
    bool skip_intrinsics = evalInt(SOP_ATTRIBUTECSVEXPORT_SKIP_INTRINSIC_ATTRIBUTES, 0, t);
    bool create_offset_attribute = evalInt(SOP_ATTRIBUTECSVEXPORT_CREATE_OFFSET_ATTRIBUTE, 0, t);
    UT_String csv_filename;

    UTgetInterrupt();

    if(lockInputs(context) >= UT_ERROR_ABORT)
    {
        return error();
    }

    duplicatePointSource(0, context);

    if(!getClassType(t, attrib_owner))
    {
        UT_WorkBuffer buf;
        buf.sprintf("Unsupported attribute class type.");
        addError(SOP_MESSAGE, buf.buffer());

        unlockInputs();
        return error();
    }

    evalString(csv_filename, SOP_ATTRIBUTECSVEXPORT_FILE, 0, t);
    if(!csv_filename || !csv_filename.length())
    {
        UT_WorkBuffer buf;
        buf.sprintf("CSV file was not specified.");
        addWarning(SOP_MESSAGE, buf.buffer());

        unlockInputs();
        return error();
    }

    UT_OFStream stream(csv_filename, UT_OFStream::trunc, UT_IOS_ASCII);
    if(!stream)
    {
        UT_WorkBuffer buf;
        buf.sprintf("Error openening CSV file for writing.");
        addError(SOP_MESSAGE, buf.buffer());

        unlockInputs();
        return error();
    }

    UT_String csv_attriborder;
    evalString(csv_attriborder, SOP_ATTRIBUTECSVEXPORT_ATTRIB_ORDER, 0, t);
    if(!csv_attriborder || !csv_attriborder.length())
    {
        UT_WorkBuffer buf;
        buf.sprintf("Attribute order was not specified.");
        addWarning(SOP_MESSAGE, buf.buffer());

        unlockInputs();
        return error();
    }
    UT_Array<UT_String> attribs;
    csv_attriborder.tokenize(attribs);
    for(UT_String s : attribs) {
        GA_Attribute* attr = gdp->findPointAttribute(s);
        if(!attr) {
            UT_WorkBuffer buf;
            buf.sprintf("Attribute %s does not exist.", s.c_str());
            addWarning(SOP_MESSAGE, buf.buffer());

            unlockInputs();
            return error();
        }
    }

    UT_Array<UT_DeepString> csv_attr_names;
    const GA_Attribute* attr = nullptr;

    if(create_offset_attribute)
    {
        createOffsetAttributeCSVNames(attrib_owner, csv_attr_names);
    }

    switch(attrib_owner)
    {
        default:
        case GA_ATTRIB_POINT:
        {
            GA_FOR_ALL_POINT_ATTRIBUTES(gdp, attr)
            {
                getAttributeCSVNames(attr, csv_attr_names, skip_intrinsics);
            }

            break;
        }

        case GA_ATTRIB_VERTEX:
        {
            GA_FOR_ALL_VERTEX_ATTRIBUTES(gdp, attr)
            {
                getAttributeCSVNames(attr, csv_attr_names, skip_intrinsics);
            }

            break;
        }

        case GA_ATTRIB_PRIMITIVE:
        {
            GA_FOR_ALL_PRIMITIVE_ATTRIBUTES(gdp, attr)
            {
                getAttributeCSVNames(attr, csv_attr_names, skip_intrinsics);
            }

            break;
        }

        case GA_ATTRIB_DETAIL:
        {
            GA_FOR_ALL_DETAIL_ATTRIBUTES(gdp, attr)
            {
                getAttributeCSVNames(attr, csv_attr_names, skip_intrinsics);
            }

            break;
        }
    }

    UT_Array<UT_DeepString> values;

    switch(attrib_owner)
    {
        default:
        case GA_ATTRIB_POINT:
        {
            GA_Offset point_offset = 0;
            GA_FOR_ALL_PTOFF(gdp, point_offset)
            {
                if(create_offset_attribute)
                {
                    createOffsetAttributeCSVValue(point_offset, values);
                }
             
                for(UT_String s : attribs) {
                    attr = gdp->findPointAttribute(s);
                    processAttributeValue(attr, point_offset, attrib_owner, skip_intrinsics, values);    
                }
                
                writeCSVValues(values, stream);
                values.clear();
            }

            break;
        }

        case GA_ATTRIB_VERTEX:
        {
            GEO_Primitive* prim = nullptr;
            GA_Offset vertex_offset = 0;
            GA_Offset prim_offset = 0;
            GA_Offset point_offset = 0;
            GA_FOR_ALL_PRIMITIVES(gdp, prim)
            {
                prim_offset = prim->getMapOffset();

                int vertex_count = prim->getVertexCount();
                for(int vtx_idx = 0; vtx_idx < vertex_count; ++vtx_idx)
                {
                    vertex_offset = prim->getVertexOffset(vtx_idx);
                    point_offset = prim->getPointOffset(vtx_idx);

                    if(create_offset_attribute)
                    {
                        createOffsetAttributeCSVValue(prim_offset, vtx_idx, values);
                        createOffsetAttributeCSVValue(point_offset, values);
                    }

                    GA_FOR_ALL_VERTEX_ATTRIBUTES(gdp, attr)
                    {
                        processAttributeValue(attr, vertex_offset, attrib_owner, skip_intrinsics, values);
                    }

                    writeCSVValues(values, stream);
                    values.clear();
                }
            }

            break;
        }

        case GA_ATTRIB_PRIMITIVE:
        {
            GA_Primitive* prim = nullptr;
            GA_Offset prim_offset = 0;
            GA_FOR_ALL_PRIMITIVES(gdp, prim)
            {
                prim_offset = prim->getMapOffset();

                if(create_offset_attribute)
                {
                    createOffsetAttributeCSVValue(prim_offset, values);
                }

                GA_FOR_ALL_PRIMITIVE_ATTRIBUTES(gdp, attr)
                {
                    processAttributeValue(attr, prim_offset, attrib_owner, skip_intrinsics, values);
                }

                writeCSVValues(values, stream);
                values.clear();
            }

            break;
        }

        case GA_ATTRIB_DETAIL:
        {
            if(create_offset_attribute)
            {
                createOffsetAttributeCSVValue(0, values);
            }

            GA_FOR_ALL_DETAIL_ATTRIBUTES(gdp, attr)
            {
                processAttributeValue(attr, 0, attrib_owner, skip_intrinsics, values);
            }

            writeCSVValues(values, stream);
            values.clear();

            break;
        }
    }

    stream.close();
    unlockInputs();
    return error();
}


const char*
SOP_AttributeCSVExport::inputLabel(unsigned int idx) const
{
    return "Input geometry.";
}


bool
SOP_AttributeCSVExport::getClassType(fpreal t, GA_AttributeOwner& attrib_owner) const
{
    int class_type = evalInt(SOP_ATTRIBUTECSVEXPORT_CLASS, 0, t);

    switch(class_type)
    {
        case 0:
        {
            attrib_owner = GA_ATTRIB_POINT;
            return true;
        }

        case 1:
        {
            attrib_owner = GA_ATTRIB_VERTEX;
            return true;
        }

        case 2:
        {
            attrib_owner = GA_ATTRIB_PRIMITIVE;
            return true;
        }

        case 3:
        {
            attrib_owner = GA_ATTRIB_DETAIL;
            return true;
        }

        default:
        {
            break;
        }
    }

    return false;
}


bool
SOP_AttributeCSVExport::createOffsetAttributeCSVNames(GA_AttributeOwner owner, UT_Array<UT_DeepString>& attr_csv_names) const
{
    switch(owner)
    {
        case GA_ATTRIB_VERTEX:
        {
            attr_csv_names.append(UT_DeepString("Offset"));
            attr_csv_names.append(UT_DeepString("Point Num"));
            return true;
        }

        case GA_ATTRIB_PRIMITIVE:
        case GA_ATTRIB_POINT:
        case GA_ATTRIB_DETAIL:
        {
            attr_csv_names.append(UT_DeepString("Offset"));
            return true;
        }

        default:
        {
            break;
        }
    }

    return false;
}


bool
SOP_AttributeCSVExport::getAttributeCSVNames(const GA_Attribute* attr, UT_Array<UT_DeepString>& attr_csv_names,
    bool skip_intrinsics) const
{
    int tuple_size = attr->getTupleSize();
    UT_DeepString attr_name(attr->getExportName());

    if(!isSupportedAttribute(attr, skip_intrinsics))
    {
        return false;
    }

    if(1 == tuple_size)
    {
        attr_csv_names.append(attr_name);
    }
    else
    {
        for(int idx = 0; idx < tuple_size; ++idx)
        {
            UT_DeepString attr_csv_name;
            attr_csv_name.sprintf("%s[%d]", attr_name.buffer(), idx);
            attr_csv_names.append(attr_csv_name);
        }
    }

    return true;
}


void
SOP_AttributeCSVExport::createOffsetAttributeCSVValue(GA_Offset offset, UT_Array<UT_DeepString>& attr_csv_values) const
{
    UT_DeepString value;
    value.sprintf("%lld", offset);
    attr_csv_values.append(value);
}


void
SOP_AttributeCSVExport::createOffsetAttributeCSVValue(GA_Offset offset1, GA_Offset offset2,
    UT_Array<UT_DeepString>& attr_csv_values) const
{
    UT_DeepString value;
    value.sprintf("%lld:%lld", offset1, offset2);
    attr_csv_values.append(value);
}


bool
SOP_AttributeCSVExport::isSupportedAttribute(const GA_Attribute* attr, bool skip_intrinsics) const
{
    UT_DeepString attr_name(attr->getExportName());
    GA_StorageClass attr_storage_class = attr->getStorageClass();

    if(GA_STORECLASS_INVALID == attr_storage_class || GA_STORECLASS_OTHER == attr_storage_class)
    {
        return false;
    }

    if(skip_intrinsics && attr_name.startsWith("."))
    {
        return false;
    }

    return true;
}


void
SOP_AttributeCSVExport::writeCSVValues(const UT_Array<UT_DeepString>& attr_csv_names, UT_OFStream& stream) const
{
    for(int idx = 0; idx < attr_csv_names.size(); ++idx)
    {
        const UT_DeepString& attr_name = attr_csv_names(idx);
        stream << attr_name;

        if(idx + 1 != attr_csv_names.size())
        {
            stream << ", ";
        }
    }

    stream << "\n";
}


void
SOP_AttributeCSVExport::processAttributeValue(const GA_Attribute* attr, GA_Offset offset, GA_AttributeOwner owner,
    bool skip_intrinsics, UT_Array<UT_DeepString>& values) const
{
    int tuple_size = attr->getTupleSize();
    UT_DeepString attr_name(attr->getExportName());
    GA_StorageClass attr_storage_class = attr->getStorageClass();

    if(!isSupportedAttribute(attr, skip_intrinsics))
    {
        return;
    }

    switch(attr_storage_class)
    {
        case GA_STORECLASS_INT:
        {
            GA_ROAttributeRef attrib(gdp->findIntTuple(owner, attr_name, tuple_size));
            GA_ROHandleI handle(attr);

            for(int idx = 0; idx < tuple_size; ++idx)
            {
                if(attrib.isValid())
                {
                    UT_DeepString value;
                    value.sprintf("%d", handle.get(offset, idx));
                    values.append(value);
                }
                else
                {
                    values.append(UT_DeepString("0"));
                }
            }

            break;
        }

        case GA_STORECLASS_REAL:
        {
            GA_ROAttributeRef attrib(gdp->findFloatTuple(owner, attr_name, tuple_size));
            GA_ROHandleF handle(attr);

            for(int idx = 0; idx < tuple_size; ++idx)
            {
                if(attrib.isValid())
                {
                    UT_DeepString value;
                    value.sprintf("%f", handle.get(offset, idx));
                    values.append(value);
                }
                else
                {
                    values.append(UT_DeepString("0.0"));
                }
            }

            break;
        }

        case GA_STORECLASS_STRING:
        {
            GA_ROAttributeRef attrib(gdp->findStringTuple(owner, attr_name, tuple_size));
            GA_ROHandleS handle(attr);

            for(int idx = 0; idx < tuple_size; ++idx)
            {
                if(attrib.isValid())
                {
                    UT_StringHolder sh = handle.get(offset, idx);
                    UT_DeepString value;
                    value.sprintf("%s", sh.buffer());
                    values.append(value);
                }
                else
                {
                    values.append(UT_DeepString(""));
                }
            }

            break;
        }

        default:
        {
            break;
        }
    }
}


void
newSopOperator(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator("attributecsvexport", "Attribute CSV Export", SOP_AttributeCSVExport::myConstructor,
        SOP_AttributeCSVExport::myTemplateList, 1, 1, 0));
}

