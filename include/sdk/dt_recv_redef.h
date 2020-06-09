#pragma once
#include "dt_common.h"
#include "tier0/dbg.h"
#include "dt_recv.h"

class RecvPropRedef
{
    // This info comes from the receive data table.
public:
    RecvPropRedef();

    void InitArray(int nElements, int elementStride);

    int GetNumElements() const;
    void SetNumElements(int nElements);

    int GetElementStride() const;
    void SetElementStride(int stride);

    int GetFlags() const;

    const char *GetName() const;
    SendPropType GetType() const;

    RecvTable *GetDataTable() const;
    void SetDataTable(RecvTable *pTable);

    RecvVarProxyFn GetProxyFn() const;
    void SetProxyFn(RecvVarProxyFn fn);

    DataTableRecvVarProxyFn GetDataTableProxyFn() const;
    void SetDataTableProxyFn(DataTableRecvVarProxyFn fn);

    int GetOffset() const;
    void SetOffset(int o);

    // Arrays only.
    RecvProp *GetArrayProp() const;
    void SetArrayProp(RecvProp *pProp);

    // Arrays only.
    void SetArrayLengthProxy(ArrayLengthRecvProxyFn proxy);
    ArrayLengthRecvProxyFn GetArrayLengthProxy() const;

    bool IsInsideArray() const;
    void SetInsideArray();

    // Some property types bind more data to the prop in here.
    const void *GetExtraData() const;
    void SetExtraData(const void *pData);

    // If it's one of the numbered "000", "001", etc properties in an array, then
    // these can be used to get its array property name for debugging.
    const char *GetParentArrayPropName();
    void SetParentArrayPropName(const char *pArrayPropName);

public:
    const char *m_pVarName;
    SendPropType m_RecvType;
    int m_Flags;
    int m_StringBufferSize;

public:
    bool m_bInsideArray; // Set to true by the engine if this property sits inside an array.

    // Extra data that certain special property types bind to the property here.
    const void *m_pExtraData;

    // If this is an array (DPT_Array).
    RecvProp *m_pArrayProp;
    ArrayLengthRecvProxyFn m_ArrayLengthProxy;

    RecvVarProxyFn m_ProxyFn;
    DataTableRecvVarProxyFn m_DataTableProxyFn; // For RDT_DataTable.

    RecvTable *m_pDataTable; // For RDT_DataTable.
    int m_Offset;

    int m_ElementStride;
    int m_nElements;

    // If it's one of the numbered "000", "001", etc properties in an array, then
    // these can be used to get its array property name for debugging.
    const char *m_pParentArrayPropName;
};
