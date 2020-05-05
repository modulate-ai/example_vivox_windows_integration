# Overview

The repository contains a skeleton of Modulate's example Vivox integration app on Windows, and is intended as a reference to show how to integrate Modulate's SDK with an application using Vivox's voice chat.

When built, the ModulateChat example application allows users to join chatrooms, and chat with each other while using voice skins.  The voice skins convert the users' voices into other voices, "digitally swapping out their vocal cords", while leaving their emotion, manner of speaking, and content preserved.  The application allows users to further customize the sound of their voice using a variety of audio filters.  Additionally, users will hear their own voice with the voice skin applied looped back into their own ears, with low enough latency to seamlessly blend their speech with the converted voice.

When being used as an integration reference, developers are encouraged to look at ModulateVivoxLibrary/ModulateVivoxIntegration.*, which contains the code linking the Vivox and Modulate SDKs.  All remaining code exists to create the UI and application behavior, link Unmanaged C++ code with C# application code, and support logging.


# Vivox and Modulate SDKs

A few classes which use example code distributed with Vivox's own SDK have been removed from this release, but references are made within the existing code as to what those hooks into the Vivox setup look like.  The removed classes contain only code relevant to using Vivox's SDK independently - with no Modulate-specific changes - and should be easily substituted by any other use of Vivox's core SDK.  Additionally, no direct links to the Vivox Core SDK are included here - please visit https://www.vivox.com/ to download the Vivox SDK.

Finally, the Modulate static library is not included in this release, although the header file defining its interface is.  Please visit https://modulate.ai for more information on obtaining the Modulate library.

Given these omissions, this repository will not build immediately when downloaded.  However, after supplying the Vivox and Modulate SDKs, and a suitable Vivox Core application in place of VivoxBase, this repository will build a functioning ModulateChat example application!


# Organization

This application breaks down into three main Modules:
* ModulateVivoxLibrary/ - contains the code linking Modulate's SDK and Vivox's SDK, and is intended to be the primary reference point for developers looking to integrate Modulate's SDK into their own voice chat applications
    * ModulateVivoxIntegration.* - A class containing the linkage between Modulate's SDK and Vivox's SDK, as well as state relating to voice skins and customization parameters
    * wav_logger.* - Functionality for logging audio from a realtime audio thread - see https://github.com/modulate-ai/wav_logger for more information
    * ModulateVivoxLibrary.* - Wrapper code for linking the Unmanaged C++ operating voice chat and voice conversion, with Managed C++ linked to the C# UI and App code
* ModulateVivoxWrapper/ - contains thin wrapper code in Managed C++, linking the Unmanaged C++ in ModulateVivoxLibrary with the C# UI and App code in ModulateChat/
* ModulateChat/ - contains the C# UI and Application code for running the ModulateChat example application
    * ModulateChat.xaml.cs - the main file in this directory, containing all of the logic surrounding logging, filesystem interaction, UI and App logic, etc.