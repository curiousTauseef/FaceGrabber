//------------------------------------------------------------------------------
// <copyright file="DirectXPanelBase.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

//	This control is based on the following article: http://msdn.microsoft.com/en-us/library/windows/apps/hh825871.aspx
//	And the code here: http://code.msdn.microsoft.com/windowsapps/XAML-SwapChainPanel-00cb688b

#pragma once
#include <concrt.h>


namespace HDFaceVisualizer
{
    
    public ref class DirectXPanelBase 
    {
    protected private:
        DirectXPanelBase();

        virtual void CreateDeviceIndependentResources();
        virtual void CreateDeviceResources();
        virtual void CreateSizeDependentResources();

        virtual void OnDeviceLost();
        virtual void OnSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
        virtual void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel ^sender, Platform::Object ^args);
        virtual void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
        virtual void OnResuming(Platform::Object^ sender, Platform::Object^ args) { };

        virtual void Render() { };
        virtual void Present();

        ID3D11Device1*                               m_d3dDevice;
        ID3D11DeviceContext1*                        m_d3dContext;
        IDXGISwapChain2*                             m_swapChain;

        ID2D1Factory2*                               m_d2dFactory;
        ID2D1Device*                                 m_d2dDevice;
        ID2D1DeviceContext*                          m_d2dContext;
        ID2D1Bitmap1*                                m_d2dTargetBitmap;

        D2D1_COLOR_F                                                        m_backgroundColor;
        DXGI_ALPHA_MODE                                                     m_alphaMode;

        bool                                                                m_loadingComplete;

        Concurrency::critical_section                                       m_criticalSection;

        float                                                               m_renderTargetHeight;
        float                                                               m_renderTargetWidth;        

        float                                                               m_compositionScaleX;
        float                                                               m_compositionScaleY;

        float                                                               m_height;
        float                                                               m_width;

    };
}
