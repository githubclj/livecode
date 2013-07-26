/* Copyright (C) 2003-2013 Runtime Revolution Ltd.
 
 This file is part of LiveCode.
 
 LiveCode is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License v3 as published by the Free
 Software Foundation.
 
 LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.
 
 You should have received a copy of the GNU General Public License
 along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"

#include "mcerror.h"
#include "globals.h"
#include "exec.h"

#include "mblstore.h"

////////////////////////////////////////////////////////////////////////////////

MC_EXEC_DEFINE_GET_METHOD(Store, CanMakePurchase, 1)
MC_EXEC_DEFINE_EXEC_METHOD(Store, EnablePurchaseUpdates, 0)
MC_EXEC_DEFINE_EXEC_METHOD(Store, DisablePurchaseUpdates, 0)
MC_EXEC_DEFINE_EXEC_METHOD(Store, RestorePurchases, 0)
MC_EXEC_DEFINE_GET_METHOD(Store, PurchaseList, 1)
MC_EXEC_DEFINE_EXEC_METHOD(Store, CreatePurchase, 1)
MC_EXEC_DEFINE_GET_METHOD(Store, PurchaseState, 2)
MC_EXEC_DEFINE_GET_METHOD(Store, PurchaseError, 2)
MC_EXEC_DEFINE_GET_METHOD(Store, PurchaseProperty, 2)
MC_EXEC_DEFINE_SET_METHOD(Store, PurchaseProperty, 3)
MC_EXEC_DEFINE_EXEC_METHOD(Store, SendPurchaseRequest, 1)
MC_EXEC_DEFINE_EXEC_METHOD(Store, ConfirmPurchaseDelivery, 1)
MC_EXEC_DEFINE_EXEC_METHOD(Store, RequestProductDetails, 1)
MC_EXEC_DEFINE_EXEC_METHOD(Store, PurchaseVerify, 2)


////////////////////////////////////////////////////////////////////////////////

static MCExecEnumTypeElementInfo _kMCStorePurchasePropertyElementInfo[] =
{
    { "product identifier", kMCPurchasePropertyProductIdentifier},
    { "quantity", kMCPurchasePropertyQuantity},
    { "developer payload", kMCPurchasePropertyDeveloperPayload},
    { "localized title", kMCPurchasePropertyLocalizedTitle},
    { "localized description", kMCPurchasePropertyLocalizedDescription},
    { "localized price", kMCPurchasePropertyLocalizedPrice},
    { "purchase date", kMCPurchasePropertyPurchaseDate},
    { "transaction identifier", kMCPurchasePropertyTransactionIdentifier},
    { "receipt", kMCPurchasePropertyReceipt},
    { "original transaction identifier", kMCPurchasePropertyOriginalTransactionIdentifier},
    { "original purchase date", kMCPurchasePropertyOriginalPurchaseDate},
    { "original receipt", kMCPurchasePropertyOriginalReceipt},
    { "signed data", kMCPurchasePropertySignedData},
    { "signature", kMCPurchasePropertySignature},
    { "unknown", kMCPurchasePropertyUnknown}
};

static MCExecEnumTypeInfo _kMCStorePurchasePropertyTypeInfo =
{
    "Store.PurchaseProperty",
    sizeof(_kMCStorePurchasePropertyElementInfo) / sizeof(MCExecEnumTypeElementInfo),
    _kMCStorePurchasePropertyElementInfo
};

MCExecEnumTypeInfo* kMCStorePurchasePropertyTypeInfo = &_kMCStorePurchasePropertyTypeInfo;

////////////////////////////////////////////////////////////////////////////////


void MCStoreGetCanMakePurchase(MCExecContext& ctxt, bool& r_result)
{
    r_result = MCStoreCanMakePurchase();
}

void MCStoreExecEnablePurchaseUpdates(MCExecContext& ctxt)
{
    if (!MCStoreEnablePurchaseUpdates())
        ctxt.Throw();
}

void MCStoreExecDisablePurchaseUpdates(MCExecContext& ctxt)
{
    if (!MCStoreDisablePurchaseUpdates())
        ctxt.Throw();
}

void MCStoreExecRestorePurchases(MCExecContext& ctxt)
{
    if (!MCStoreRestorePurchases())
        ctxt.Throw();
}


void MCStoreGetPurchaseList(MCExecContext& ctxt, MCStringRef& r_list)
{
    if (!MCPurchaseList(ctxt.GetEP()))
        ctxt.Throw();
    else
    {
        /*UNCHECKED*/ ctxt.GetEP().copyasstringref(r_list);
    }
}

void MCStoreExecCreatePurchase(MCExecContext& ctxt, MCStringRef p_product_id, uint32_t& r_id)
{
    MCPurchase *t_purchase = nil;
    bool t_success = true;
    
    t_success = MCPurchaseCreate(p_product_id, nil, t_purchase);
    
    if (t_success)
        r_id = t_purchase->id;
    else
        ctxt.Throw();
}

void MCStoreGetPurchaseState(MCExecContext& ctxt, int p_id, MCStringRef& r_state)
{
    const char* t_state = nil;
	MCPurchase *t_purchase = nil;
    bool t_success = true;
    
	t_success = MCPurchaseFindById(p_id, t_purchase);
    
	if (t_success)
		t_success = MCPurchaseStateToString(t_purchase->state, t_state);
	
	if (t_success)
        t_success = MCStringCreateWithCString(t_state, r_state);
    
    if (t_success)
        return;
    
    ctxt.Throw();
}

void MCStoreGetPurchaseError(MCExecContext& ctxt, int p_id, MCStringRef& r_error)
{
    bool t_success = true;
	MCAutoStringRef t_error;
	MCPurchase *t_purchase = nil;
    
    t_success = MCPurchaseFindById(p_id, t_purchase);
	
    if (t_success)
        t_success = (t_purchase == nil || t_purchase->state != kMCPurchaseStateError);
    
	if (t_success)
		t_success = MCPurchaseGetError(t_purchase, &t_error);
	
	if (t_success)
        if (MCStringCopy(*t_error, r_error))
            return;
    
    ctxt.Throw();
}
/*
void MCStoreGetPurchaseProperty(MCExecContext& ctxt, int p_id, MCStringRef p_prop_name)
{
	MCPurchase *t_purchase = nil;
	MCPurchaseProperty t_property;
    bool t_success = true;
    
    t_success =
        MCPurchaseFindById(p_id, t_purchase) &&
        MCPurchaseLookupProperty(p_prop_name, t_property);
    
	if (t_success)
		t_success = MCPurchaseGet(t_purchase, t_property, ctxt. GetEP()) == ES_NORMAL;
    
    if (t_success)
        return;
    
    ctxt . Throw();

}

void MCStoreSetPurchaseProperty(MCExecContext& ctxt, int p_id, MCStringRef p_prop_name, uint32_t p_quantity)
{
	MCPurchase *t_purchase = nil;
	MCPurchaseProperty t_property;
    bool t_success = true;
    
    t_success =
        MCPurchaseFindById(p_id, t_purchase) &&
        MCPurchaseLookupProperty(p_prop_name, t_property);
    
	if (t_success)
		t_success = MCPurchaseSet(t_purchase, t_property, p_quantity) == ES_NORMAL;
    
    if (t_success)
        return;
    else
        ctxt.Throw();    
}
*/

MCPurchasePropertyInfo *lookup_purchase_property(const MCPurchasePropertyTable *p_table, MCPurchaseProperty p_which)
{
	for(uindex_t i = 0; i < p_table -> size; i++)
		if (p_table -> table[i] . property == p_which)
			return &p_table -> table[i];
	
	return nil;
}


void MCStoreExecGet(MCExecContext& ctxt, integer_t p_id, MCStringRef p_prop_name, MCValueRef& r_value)
{
    MCPurchase *t_purchase = nil;
	MCPurchaseProperty t_property;
    
    MCPurchaseFindById(p_id, t_purchase) &&
    MCPurchaseLookupProperty(p_prop_name, t_property);
    
    MCPurchasePropertyInfo *t_info;
    t_info = lookup_purchase_property(getpropertytable(), t_property);
    
    MCExecPoint& ep = ctxt . GetEP();
    ep . clear();
    
	if (t_info != nil)
	{
		switch(t_info -> type)
		{
			case kMCPropertyTypeAny:
			{
				MCAutoValueRef t_any;
				((void(*)(MCPurchase*, MCValueRef&))t_info -> getter)(t_purchase, &t_any);
				if (!ctxt . HasError())
				{
					ep . setvalueref(*t_any);
//					return ES_NORMAL;
				}
			}
                break;
				
			case kMCPropertyTypeBool:
			{
				bool t_value;
				((void(*)(MCPurchase*, bool&))t_info -> getter)(t_purchase, t_value);
				if (!ctxt . HasError())
				{
					ep . setboolean(t_value ? True : False);
//					return ES_NORMAL;
				}
			}
                break;
                
			case kMCPropertyTypeInt16:
			case kMCPropertyTypeInt32:
			{
				integer_t t_value;
				((void(*)(MCPurchase*, integer_t&))t_info -> getter)(t_purchase, t_value);
				if (!ctxt . HasError())
				{
					ep . setint(t_value);
//					return ES_NORMAL;
				}
			}
                break;
                
			case kMCPropertyTypeUInt16:
			case kMCPropertyTypeUInt32:
			{
				uinteger_t t_value;
				((void(*)(MCPurchase*, uinteger_t&))t_info -> getter)(t_purchase, t_value);
				if (!ctxt . HasError())
				{
					ep . setuint(t_value);
//					return ES_NORMAL;
				}
			}
                break;
				
			case kMCPropertyTypeDouble:
			{
				double t_value;
				((void(*)(MCPurchase*, double&))t_info -> getter)(t_purchase, t_value);
				if (!ctxt . HasError())
				{
					ep . setnvalue(t_value);
//					return ES_NORMAL;
				}
			}
                break;
                
			case kMCPropertyTypeChar:
			{
				char_t t_value;
				((void(*)(MCPurchase*, char_t&))t_info -> getter)(t_purchase, t_value);
				if (!ctxt . HasError())
				{
					ep . setchar((char)t_value);
//					return ES_NORMAL;
				}
			}
                break;
				
			case kMCPropertyTypeString:
			case kMCPropertyTypeBinaryString:
			{
				MCAutoStringRef t_value;
				((void(*)(MCPurchase*, MCStringRef&))t_info -> getter)(t_purchase, &t_value);
				if (!ctxt . HasError())
				{
					ep . setvalueref(*t_value);
//					return ES_NORMAL;
				}
			}
                break;
				
			default:
				MCAssert(false);
				break;
		}
	}
    
    if (!ctxt . HasError())
        ep . copyasvalueref(r_value);
}

void MCStoreExecSet(MCExecContext& ctxt, integer_t p_id, MCStringRef p_prop_name, MCValueRef r_value)
{
    MCPurchase *t_purchase = nil;
	MCPurchaseProperty t_property;
    
    MCPurchaseFindById(p_id, t_purchase) &&
    MCPurchaseLookupProperty(p_prop_name, t_property);
    
    MCPurchasePropertyInfo *t_info;
    t_info = lookup_purchase_property(getpropertytable(), t_property);
    
    MCExecPoint& ep = ctxt . GetEP();
    ep . setvalueref(r_value);
    
	if (t_info != nil)
	{
		switch(t_info -> type)
		{
			case kMCPropertyTypeAny:
			{
				MCAutoValueRef t_value;
				/* UNCHECKED */ ep . copyasvalueref(&t_value);
				((void(*)(MCPurchase*, MCValueRef))t_info -> setter)(t_purchase, *t_value);
			}
                break;
				
			case kMCPropertyTypeBool:
			{
				bool t_value;
				if (!ep . copyasbool(t_value))
					ctxt . LegacyThrow(EE_PROPERTY_NAB);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, bool))t_info -> setter)(t_purchase, t_value);
			}
                break;
                
			case kMCPropertyTypeInt16:
			{
				integer_t t_value;
				if (!ep . copyasint(t_value) ||
					t_value < -32768 || t_value > 32767)
					ctxt . LegacyThrow(EE_PROPERTY_NAN);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, integer_t))t_info -> setter)(t_purchase, t_value);
			}
                break;
				
			case kMCPropertyTypeInt32:
			{
				integer_t t_value;
				if (!ep . copyasint(t_value))
					ctxt . LegacyThrow(EE_PROPERTY_NAN);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, integer_t))t_info -> setter)(t_purchase, t_value);
			}
                break;
                
			case kMCPropertyTypeUInt16:
			{
				uinteger_t t_value;
				if (!ep . copyasuint(t_value) ||
					t_value < 0 || t_value > 65535)
					ctxt . LegacyThrow(EE_PROPERTY_NAN);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, uinteger_t))t_info -> setter)(t_purchase, t_value);
			}
                break;
				
			case kMCPropertyTypeUInt32:
			{
				uinteger_t t_value;
				if (!ep . copyasuint(t_value))
					ctxt . LegacyThrow(EE_PROPERTY_NAN);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, uinteger_t))t_info -> setter)(t_purchase, t_value);
			}
                break;
				
			case kMCPropertyTypeDouble:
			{
				double t_value;
				if (!ep . copyasdouble(t_value))
					ctxt . LegacyThrow(EE_PROPERTY_NAN);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, double))t_info -> setter)(t_purchase, t_value);
			}
                break;
                
			case kMCPropertyTypeChar:
			{
				char_t t_value;
				if (!ep . copyaschar(t_value))
					ctxt . LegacyThrow(EE_PROPERTY_NAC);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, char_t))t_info -> setter)(t_purchase, t_value);
			}
                break;
				
			case kMCPropertyTypeString:
			case kMCPropertyTypeBinaryString:
			{
				MCAutoStringRef t_value;
				if (!ep . copyasstringref(&t_value))
					ctxt . LegacyThrow(EE_PROPERTY_NAC);
				if (!ctxt . HasError())
					((void(*)(MCPurchase*, MCStringRef))t_info -> setter)(t_purchase, *t_value);
			}
                break;
				
			default:
				ctxt . Unimplemented();
				break;
		}
	}
}

void MCStoreExecSendPurchaseRequest(MCExecContext& ctxt, uint32_t p_id)
{
	MCPurchase *t_purchase = nil;
    bool t_success = true;
    
    t_success = MCPurchaseFindById(p_id, t_purchase);
	
	if (t_success)
		t_success = MCPurchaseSendRequest(t_purchase);
    
    if (t_success)
        return;
    else
        ctxt.Throw();
}

void MCStoreExecConfirmPurchaseDelivery(MCExecContext& ctxt, uint32_t p_id)
{
	MCPurchase *t_purchase = nil;
    bool t_success = true;
    
    t_success = MCPurchaseFindById(p_id, t_purchase);
    
    if (t_success)
        t_success = (!(t_purchase->state == kMCPurchaseStatePaymentReceived || t_purchase->state == kMCPurchaseStateRefunded || t_purchase->state == kMCPurchaseStateRestored));
	
	if (t_success)
		t_success = MCPurchaseConfirmDelivery(t_purchase);
    
    if (t_success)
        return;
    else
        ctxt.Throw();
}

void MCStoreExecRequestProductDetails(MCExecContext& ctxt, MCStringRef p_product_id)
{
    if (MCStoreRequestProductDetails(p_product_id))
        return;
    
    ctxt.Throw();
}

void MCStoreExecPurchaseVerify(MCExecContext& ctxt, uint32_t p_id, bool p_verified)
{
    MCPurchase *t_purchase = nil;
    bool t_success = true;    
    
    t_success = MCPurchaseFindById(p_id, t_purchase);
    
    if (t_success)
        MCPurchaseVerify(t_purchase, p_verified);

    if (t_success)
        return;
    
    ctxt.Throw();
}
