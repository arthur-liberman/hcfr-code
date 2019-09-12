/* libusb-win32, Generic Windows USB Library
 * Copyright (c) 2002-2005 Stephan Meyer <ste_meyer@web.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "libusb_driver.h"
#include <stdlib.h>


NTSTATUS set_configuration(libusb_device_t *dev,
						   int configuration,
                           int timeout)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB urb, *urb_ptr = NULL;
    USB_CONFIGURATION_DESCRIPTOR *configuration_descriptor = NULL;
    USB_INTERFACE_DESCRIPTOR *interface_descriptor = NULL;
    USBD_INTERFACE_LIST_ENTRY *interfaces = NULL;
    int i, j, interface_number, desc_size, config_index, ret;

	// check if this config value is already set
	if ((configuration > 0) && dev->config.value == configuration)
    {
        return STATUS_SUCCESS;
    }

	// check if this config index is already set
	if ((configuration < 0) && dev->config.value && dev->config.index == (abs(configuration)-1))
    {
        return STATUS_SUCCESS;
    }

    memset(&urb, 0, sizeof(URB));

    if (configuration == 0)
    {
        urb.UrbHeader.Function = URB_FUNCTION_SELECT_CONFIGURATION;
        urb.UrbHeader.Length = sizeof(struct _URB_SELECT_CONFIGURATION);

        status = call_usbd(dev, &urb, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

        if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb.UrbHeader.Status))
        {
            USBERR("setting configuration %d failed: status: 0x%x, urb-status: 0x%x\n",
                        configuration, status, urb.UrbHeader.Status);
            return status;
        }

        dev->config.handle =  urb.UrbSelectConfiguration.ConfigurationHandle;
        dev->config.value = 0;

        clear_pipe_info(dev);

        return status;
    }

	if (configuration <= SET_CONFIG_ACTIVE_CONFIG)
	{
		// note: as of v1.2.4.0, the active/default configuration is 
		// always the first configuration the device returns. (index 0)
		configuration=-1;
	}

	USBMSG("setting configuration %s %d timeout=%d",
		(configuration < 0) ? "index" : "value",
		(configuration < 0) ? abs(configuration) - 1 : configuration,
		timeout);


	// If configuration is negative, it is retrieved by index. 
	//
    configuration_descriptor = get_config_descriptor(dev, configuration,
                               &desc_size, &config_index);
    if (!configuration_descriptor)
    {
        USBERR0("getting configuration descriptor failed");
        return STATUS_INVALID_PARAMETER;
    }

	// if we passed an index in we can check here to see
	// if the device is already configured with this value
	if (dev->config.value == configuration_descriptor->bConfigurationValue)
    {
		UpdateContextConfigDescriptor(
			dev, 
			configuration_descriptor, 
			desc_size, 
			configuration_descriptor->bConfigurationValue, 
			config_index);

		status = STATUS_SUCCESS;
		goto SetConfigurationDone;
    }

	// MEMORY ALLOCATION BEGINS
    interfaces =
        ExAllocatePool(NonPagedPool,(configuration_descriptor->bNumInterfaces + 1)
                       * sizeof(USBD_INTERFACE_LIST_ENTRY));

    if (!interfaces)
    {
        USBERR0("memory allocation failed\n");
		status = STATUS_NO_MEMORY;
		ExFreePool(configuration_descriptor);
		goto SetConfigurationDone;
    }

    memset(interfaces, 0, (configuration_descriptor->bNumInterfaces + 1)
           * sizeof(USBD_INTERFACE_LIST_ENTRY));

    interface_number = 0;

    for (i = 0; i < configuration_descriptor->bNumInterfaces; i++)
    {
        for (j = interface_number; j < LIBUSB_MAX_NUMBER_OF_INTERFACES; j++)
        {
            interface_descriptor =
                find_interface_desc(configuration_descriptor, desc_size, j, 0);
            if (interface_descriptor)
            {
                interface_number = ++j;
                break;
            }
        }

        if (!interface_descriptor)
        {
            USBERR("unable to find interface descriptor at index %d\n", i);
			status = STATUS_INVALID_PARAMETER;
			ExFreePool(configuration_descriptor);
			goto SetConfigurationDone;
        }
        else
        {
            USBMSG("found interface %d\n",
                          interface_descriptor->bInterfaceNumber);
            interfaces[i].InterfaceDescriptor = interface_descriptor;
        }
    }

	urb_ptr = USBD_CreateConfigurationRequestEx(configuration_descriptor, interfaces);
	if (!urb_ptr)
	{
		USBERR0("memory allocation failed\n");
		status = STATUS_NO_MEMORY;
		ExFreePool(configuration_descriptor);
		goto SetConfigurationDone;
	}

	for (i = 0; i < configuration_descriptor->bNumInterfaces; i++)
	{
		for (j = 0; j < (int)interfaces[i].Interface->NumberOfPipes; j++)
		{
			interfaces[i].Interface->Pipes[j].MaximumTransferSize = LIBUSB_MAX_READ_WRITE;
		}
	}

	USBDBG("#%d %s passing configuration request to target-device.",
		dev->id, dev->device_id);

	status = call_usbd(dev, urb_ptr, IOCTL_INTERNAL_USB_SUBMIT_URB, timeout);

	if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb_ptr->UrbHeader.Status))
	{
		USBERR("setting configuration %d failed: status: 0x%x, urb-status: 0x%x\n",
					configuration, status, urb_ptr->UrbHeader.Status);
		if (NT_SUCCESS(status)) status = urb_ptr->UrbHeader.Status;

		ExFreePool(configuration_descriptor);
		goto SetConfigurationDone;
	}

	dev->config.handle = urb_ptr->UrbSelectConfiguration.ConfigurationHandle;

    clear_pipe_info(dev);

    for (i = 0; i < configuration_descriptor->bNumInterfaces; i++)
    {
        update_pipe_info(dev, interfaces[i].Interface);
    }
	UpdateContextConfigDescriptor(dev, configuration_descriptor, desc_size, configuration_descriptor->bConfigurationValue, config_index);

SetConfigurationDone:
    if (interfaces)
		ExFreePool(interfaces);

    if (urb_ptr)
		ExFreePool(urb_ptr);

    return status;
}
