#include "obs-qsv-onevpl-encoder-internal.hpp"

mfxU16 mfx_OpenEncodersNum = 0;
mfxHDL mfx_DX_Handle = nullptr;

QSV_VPL_Encoder_Internal::QSV_VPL_Encoder_Internal(mfxVersion &version,
                                                   bool isDGPU)
    : mfx_Impl(MFX_IMPL_HARDWARE_ANY), mfx_Platform({}), mfx_Version({}),
      mfx_Loader(0), mfx_LoaderConfig(0), mfx_LoaderVariant({0}),
      mfx_Session(nullptr), mfx_EncodeSurface(nullptr), mfx_VideoENC(nullptr),
      VPS_Buffer(), SPS_Buffer(), PPS_Buffer(), VPS_BufferSize(128),
      SPS_BufferSize(512), PPS_BufferSize(128), mfx_Bitstream({}),
      b_isDGPU(isDGPU), mfx_ResetParams({}),
      ResetParamChanged(false), mfx_SyncPoint(nullptr),
      mfx_BufferedSyncPoint(nullptr) {
  mfxIMPL tempImpl = MFX_IMPL_VIA_D3D11;
  isD3D11 = true;
#if defined(__linux__)
  tempImpl = MFX_IMPL_VIA_VAAPI;
  isD3D11 = false;
#endif
  mfx_Loader = MFXLoad();
  mfxStatus sts = MFXCreateSession(mfx_Loader, 0, &mfx_Session);
  blog(LOG_INFO, "Session init status: %d", sts);
  if (sts == MFX_ERR_NONE) {
    sts = MFXQueryVersion(mfx_Session, &version);
    blog(LOG_INFO, "Version query status: %d", sts);
    if (sts == MFX_ERR_NONE) {
      mfx_Version = version;
      sts = MFXQueryIMPL(mfx_Session, &tempImpl);
      blog(LOG_INFO, "Impl query status: %d", sts);
      if (sts == MFX_ERR_NONE) {
        mfx_Impl = tempImpl;
        blog(LOG_INFO,
             "\tImplementation:           %s\n"
             "\tsurf:           %s\n",
             isD3D11 ? "D3D11" : "VA-API", isD3D11 ? "D3D11" : "VA-API");
      }
    } else {
      blog(LOG_INFO, "\tImplementation: [ERROR]\n");
    }
  } else {
    blog(LOG_INFO, "\tImplementation: [ERROR]\n");
  }
  MFXClose(mfx_Session);
  MFXUnload(mfx_Loader);
}

QSV_VPL_Encoder_Internal::~QSV_VPL_Encoder_Internal() {
  if (mfx_VideoENC) {
    ClearData();
  }
}
mfxStatus QSV_VPL_Encoder_Internal::Initialize(int deviceNum) {
  mfxStatus sts = MFX_ERR_NONE;

  // Initialize VPL Session
  mfx_Loader = MFXLoad();
  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.Impl.mfxImplType"),
      mfx_LoaderVariant);

  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = static_cast<mfxU32>(0x8086);
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.VendorID"),
      mfx_LoaderVariant);

#if defined(_WIN32) || defined(_WIN64)
  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_PTR;
  mfx_LoaderVariant.Data.Ptr = mfxHDL("mfx-gen");
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.ImplName"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>(
          "mfxImplDescription.mfxAccelerationModeDescription.Mode"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_HANDLE_D3D11_DEVICE;
  MFXSetConfigFilterProperty(mfx_LoaderConfig,
                             reinterpret_cast<const mfxU8 *>("mfxHandleType"),
                             mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
  mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_ON;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig, reinterpret_cast<const mfxU8 *>("mfxInitParam.GPUCopy"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
  mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_ON;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxInitializationParam.DeviceCopy"),
      mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U16;
  mfx_LoaderVariant.Data.U16 = MFX_GPUCOPY_ON;
  MFXSetConfigFilterProperty(mfx_LoaderConfig,
                             reinterpret_cast<const mfxU8 *>("DeviceCopy"),
                             mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
  MFXSetConfigFilterProperty(mfx_LoaderConfig,
                             reinterpret_cast<const mfxU8 *>(
                                 "mfxInitializationParam.AccelerationMode"),
                             mfx_LoaderVariant);

  mfx_LoaderConfig = MFXCreateConfig(mfx_Loader);
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_PRIORITY_HIGH;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>(
          "mfxInitializationParam.mfxExtThreadsParam.Priority"),
      mfx_LoaderVariant);

#elif defined(__linux__)
  mfx_LoaderVariant.Type = MFX_VARIANT_TYPE_U32;
  mfx_LoaderVariant.Data.U32 = MFX_ACCEL_MODE_VIA_VAAPI_DRM_RENDER_NODE;
  MFXSetConfigFilterProperty(
      mfx_LoaderConfig,
      reinterpret_cast<const mfxU8 *>("mfxImplDescription.AccelerationMode"),
      mfx_LoaderVariant);

  mfxHDL vaDisplay = nullptr;
  if (obs_get_nix_platform() == OBS_NIX_PLATFORM_X11_EGL) {
    vaDisplay = vaGetDisplay(static_cast<Display*>(obs_get_nix_platform_display()));
  } else if (obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND) {
    vaDisplay = vaGetDisplayWl(static_cast<wl_display*>(obs_get_nix_platform_display()));
  }
#endif

  sts = MFXCreateSession(mfx_Loader, deviceNum, &mfx_Session);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "CreateSession error: %d", sts);
    return sts;
  }

  sts = MFXQueryIMPL(mfx_Session, &mfx_Impl);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "QueryIMPL error: %d", sts);
    return sts;
  }

  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "SetHandle error: %d", sts);
    return sts;
  }
  // PRAGMA_WARN_PUSH
  // PRAGMA_WARN_DEPRECATION
  MFXVideoCORE_QueryPlatform(mfx_Session, &mfx_Platform);
  if (mfx_Platform.MediaAdapterType == MFX_MEDIA_DISCRETE) {
    blog(LOG_INFO, "\tAdapter type: Discrete");
    b_isDGPU = true;
  } else {
    blog(LOG_INFO, "\tAdapter type: Integrate");
    b_isDGPU = false;
  }
  // PRAGMA_WARN_POP

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Open(struct qsv_param_t *pParams,
                                         enum qsv_codec codec) {
  mfxStatus sts = Initialize(pParams->nDeviceNum);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Open.Initialize error: %d", sts);
    return sts;
  }

  mfx_VideoENC = new MFXVideoENCODE(mfx_Session);

  sts = InitENCParams(pParams, codec);
  if (sts < MFX_ERR_NONE && sts != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
    blog(LOG_WARNING, "Open.Query error: %d", sts);
    return sts;
  }

  // InitENCCtrlParams(pParams, codec);

#if 1
  mfxFrameAllocRequest EncRequest;
  memset(&EncRequest, 0, sizeof(EncRequest));
  mfx_VideoENC->QueryIOSurf(&mfx_EncParams, &EncRequest);
  blog(LOG_WARNING, "SurfNum: %d, %d", EncRequest.NumFrameSuggested,
       EncRequest.NumFrameMin);
#endif

  sts = mfx_VideoENC->Init(&mfx_EncParams);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Open.Init error: %d", sts);
    return sts;
  }

  // sts = mfx_VideoENC->GetVideoParam(&mfx_EncParams);
  // if (sts < MFX_ERR_NONE) {
  //	blog(LOG_WARNING, "Open.GetVideoParam error: %d", sts);
  //	return sts;
  //

  sts = GetVideoParam(codec);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Open.GetVideoParam error: %d", sts);
    return sts;
  }

  sts = mfx_Bitstream.Init((mfx_EncParams.mfx.BufferSizeInKB * 1000 *
                            mfx_EncParams.mfx.BRCParamMultiplier * 1000));
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Open.Bitstream.Init error: %d", sts);
    return sts;
  }

  if (sts >= MFX_ERR_NONE) {
    mfx_OpenEncodersNum++;
  } else {
    blog(LOG_INFO, "\tOpen encoder: [ERROR]");
    ClearData();
  }

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::InitENCParams(struct qsv_param_t *pParams,
                                                  enum qsv_codec codec) {
  /*It's only for debug*/
  bool mfx_Ext_CO_enable = 1;
  bool mfx_Ext_CO2_enable = 1;
  bool mfx_Ext_CO3_enable = 1;
  bool mfx_Ext_CO_DDI_enable = 1;

  switch (codec) {
  case QSV_CODEC_AV1:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_AV1;
    break;
  case QSV_CODEC_HEVC:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_HEVC;
    break;
  case QSV_CODEC_VP9:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_VP9;
    break;
  case QSV_CODEC_AVC:
  default:
    mfx_EncParams.mfx.CodecId = MFX_CODEC_AVC;
    break;
  }

  // Width must be a multiple of 16
  // Height must be a multiple of 16 in case of frame picture and a
  // multiple of 32 in case of field picture
  mfx_EncParams.mfx.FrameInfo.Width =
      static_cast<mfxU16>((((pParams->nWidth + 15) >> 4) << 4));
  blog(LOG_INFO, "\tWidth: %d", mfx_EncParams.mfx.FrameInfo.Width);

  mfx_EncParams.mfx.FrameInfo.Height =
      static_cast<mfxU16>((((pParams->nHeight + 15) >> 4) << 4));
  blog(LOG_INFO, "\tHeight: %d", mfx_EncParams.mfx.FrameInfo.Height);

  mfx_EncParams.mfx.FrameInfo.ChromaFormat =
      static_cast<mfxU16>(pParams->nChromaFormat);

  mfx_EncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  mfx_EncParams.mfx.FrameInfo.CropX = 0;

  mfx_EncParams.mfx.FrameInfo.CropY = 0;

  mfx_EncParams.mfx.FrameInfo.CropW = static_cast<mfxU16>(pParams->nWidth);

  mfx_EncParams.mfx.FrameInfo.CropH = static_cast<mfxU16>(pParams->nHeight);

  mfx_EncParams.mfx.FrameInfo.FrameRateExtN =
      static_cast<mfxU32>(pParams->nFpsNum);

  mfx_EncParams.mfx.FrameInfo.FrameRateExtD =
      static_cast<mfxU32>(pParams->nFpsDen);

  mfx_EncParams.mfx.FrameInfo.FourCC = static_cast<mfxU32>(pParams->nFourCC);

  mfx_EncParams.mfx.FrameInfo.BitDepthChroma =
      pParams->video_fmt_10bit ? 10 : 8;

  mfx_EncParams.mfx.FrameInfo.BitDepthLuma = pParams->video_fmt_10bit ? 10 : 8;

  if (pParams->video_fmt_10bit) {
    mfx_EncParams.mfx.FrameInfo.Shift = 1;
  }

  mfx_EncParams.mfx.LowPower = GetCodingOpt(pParams->bLowpower);
  blog(LOG_INFO, "\tLowpower set: %s",
       GetCodingOptStatus(mfx_EncParams.mfx.LowPower).c_str());

  mfx_EncParams.mfx.RateControlMethod =
      static_cast<mfxU16>(pParams->RateControl);

  if ((pParams->nNumRefFrame > 0) && (pParams->nNumRefFrame < 17)) {
    if (codec == QSV_CODEC_AVC && pParams->bLookahead == true &&
        (pParams->nNumRefFrame < pParams->nGOPRefDist - 1)) {
      pParams->nNumRefFrame = pParams->nGOPRefDist;
      blog(LOG_WARNING, "\tThe AVC codec using Lookahead may be unstable if "
                        "NumRefFrame < GopRefDist. The NumRefFrame value is "
                        "automatically set to the GopRefDist value");
    }
    mfx_EncParams.mfx.NumRefFrame = static_cast<mfxU16>(pParams->nNumRefFrame);
    blog(LOG_INFO, "\tNumRefFrame set to: %d", pParams->nNumRefFrame);
  }

  mfx_EncParams.mfx.TargetUsage = static_cast<mfxU16>(pParams->nTargetUsage);
  mfx_EncParams.mfx.CodecProfile = static_cast<mfxU16>(pParams->CodecProfile);
  if (codec == QSV_CODEC_HEVC) {
    mfx_EncParams.mfx.CodecProfile |= pParams->HEVCTier;
  }

  /*This is a multiplier to bypass the limitation of the 16 bit value of
          variables*/
  mfx_EncParams.mfx.BRCParamMultiplier = 100;

  switch (pParams->RateControl) {
  case MFX_RATECONTROL_CBR:
    mfx_EncParams.mfx.TargetKbps = static_cast<mfxU16>(pParams->nTargetBitRate);

    mfx_EncParams.mfx.BufferSizeInKB =
        (pParams->bLookahead == true)
            ? static_cast<mfxU16>(
                  (mfx_EncParams.mfx.TargetKbps / static_cast<float>(8)) /
                  (static_cast<float>(
                       mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                   mfx_EncParams.mfx.FrameInfo.FrameRateExtD) *
                  (pParams->nLADepth +
                   (static_cast<float>(
                        mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                    mfx_EncParams.mfx.FrameInfo.FrameRateExtD)))
            : static_cast<mfxU16>((mfx_EncParams.mfx.TargetKbps / 8) * 1);
    if (pParams->bCustomBufferSize == true && pParams->nBufferSize > 0) {
      mfx_EncParams.mfx.BufferSizeInKB =
          static_cast<mfxU16>(pParams->nBufferSize);
      blog(LOG_INFO, "\tCustomBufferSize set: ON");
    }
    mfx_EncParams.mfx.InitialDelayInKB =
        static_cast<mfxU16>(mfx_EncParams.mfx.BufferSizeInKB / 2);
    blog(LOG_INFO, "\tBufferSize set to: %d KB",
         mfx_EncParams.mfx.BufferSizeInKB * 100);
    mfx_EncParams.mfx.MaxKbps = mfx_EncParams.mfx.TargetKbps;
    break;
  case MFX_RATECONTROL_VBR:
    mfx_EncParams.mfx.TargetKbps = static_cast<mfxU16>(pParams->nTargetBitRate);
    mfx_EncParams.mfx.MaxKbps = static_cast<mfxU16>(pParams->nMaxBitRate);
    mfx_EncParams.mfx.BufferSizeInKB =
        (pParams->bLookahead == true)
            ? static_cast<mfxU16>(
                  (mfx_EncParams.mfx.TargetKbps / static_cast<float>(8)) /
                  (static_cast<float>(
                       mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                   mfx_EncParams.mfx.FrameInfo.FrameRateExtD) *
                  (pParams->nLADepth +
                   (static_cast<float>(
                        mfx_EncParams.mfx.FrameInfo.FrameRateExtN) /
                    mfx_EncParams.mfx.FrameInfo.FrameRateExtD)))
            : static_cast<mfxU16>((mfx_EncParams.mfx.TargetKbps / 8) * 1);
    if (pParams->bCustomBufferSize == true && pParams->nBufferSize > 0) {
      mfx_EncParams.mfx.BufferSizeInKB =
          static_cast<mfxU16>(pParams->nBufferSize);
      blog(LOG_INFO, "\tCustomBufferSize set: ON");
    }
    mfx_EncParams.mfx.InitialDelayInKB =
        static_cast<mfxU16>(mfx_EncParams.mfx.BufferSizeInKB / 2);
    blog(LOG_INFO, "\tBufferSize set to: %d KB",
         mfx_EncParams.mfx.BufferSizeInKB * 100);
    break;
  case MFX_RATECONTROL_CQP:
    mfx_EncParams.mfx.QPI = static_cast<mfxU16>(pParams->nQPI);
    mfx_EncParams.mfx.QPB = static_cast<mfxU16>(pParams->nQPB);
    mfx_EncParams.mfx.QPP = static_cast<mfxU16>(pParams->nQPP);
    break;
  case MFX_RATECONTROL_ICQ:
    mfx_EncParams.mfx.ICQQuality = static_cast<mfxU16>(pParams->nICQQuality);
    break;
  }

  mfx_EncParams.AsyncDepth = static_cast<mfxU16>(pParams->nAsyncDepth);

  mfx_EncParams.mfx.GopPicSize =
      (pParams->nKeyIntSec > 0)
          ? static_cast<mfxU16>(
                pParams->nKeyIntSec *
                mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
                static_cast<float>(mfx_EncParams.mfx.FrameInfo.FrameRateExtD))
          : 240;

  if ((!pParams->bAdaptiveI && !pParams->bAdaptiveB) ||
      pParams->bAdaptiveI == false && pParams->bAdaptiveB == false) {
    mfx_EncParams.mfx.GopOptFlag = MFX_GOP_STRICT;
    blog(LOG_INFO, "\tGopOptFlag set: STRICT");
  } else {
    mfx_EncParams.mfx.GopOptFlag = MFX_GOP_CLOSED;
    blog(LOG_INFO, "\tGopOptFlag set: CLOSED");
  }

  switch (codec) {
  case QSV_CODEC_HEVC:
    mfx_EncParams.mfx.IdrInterval = 1;
    mfx_EncParams.mfx.NumSlice = 0;
    break;
  default:
    mfx_EncParams.mfx.NumSlice = 1;
    break;
  }

  mfx_EncParams.mfx.GopRefDist = static_cast<mfxU16>(pParams->nGOPRefDist);

  if (codec == QSV_CODEC_AV1 && pParams->bLookahead == false &&
      pParams->bEncTools == true &&
      (mfx_EncParams.mfx.GopRefDist != 1 && mfx_EncParams.mfx.GopRefDist != 2 &&
       mfx_EncParams.mfx.GopRefDist != 4 && mfx_EncParams.mfx.GopRefDist != 8 &&
       mfx_EncParams.mfx.GopRefDist != 16)) {
    mfx_EncParams.mfx.GopRefDist = 8;
    blog(LOG_WARNING,
         "\tThe AV1 codec without Lookahead cannot be used with EncTools if "
         "GopRefDist does not "
         "match the values 1, 2, 4, 8, 16. GOPRefDist automaticaly set to 8.");
  }
  blog(LOG_INFO, "\tGOPRefDist set to: %d frames (%d b-frames)",
       (int)mfx_EncParams.mfx.GopRefDist,
       (int)(mfx_EncParams.mfx.GopRefDist == 0
                 ? 0
                 : mfx_EncParams.mfx.GopRefDist - 1));

  if (mfx_Ext_CO_enable == 1 && codec != QSV_CODEC_VP9) {
    auto CO = mfx_EncParams.AddExtBuffer<mfxExtCodingOption>();
    /*Don't touch it!*/
    CO->CAVLC = MFX_CODINGOPTION_OFF;
    CO->SingleSeiNalUnit = MFX_CODINGOPTION_ON;
    CO->RefPicMarkRep = MFX_CODINGOPTION_ON;
    CO->PicTimingSEI = MFX_CODINGOPTION_ON;
    CO->AUDelimiter = MFX_CODINGOPTION_OFF;

    CO->ResetRefList = MFX_CODINGOPTION_ON;
    //CO->FieldOutput = MFX_CODINGOPTION_ON;
    CO->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
    CO->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
    CO->MVPrecision = MFX_MVPRECISION_QUARTERPEL;
    CO->MECostType = static_cast<mfxU16>(8);
    CO->MESearchType = static_cast<mfxU16>(16);
    CO->MVSearchWindow.x = (codec == QSV_CODEC_AVC) ? static_cast<mfxI16>(16)
                                                    : static_cast<mfxI16>(32);
    CO->MVSearchWindow.y = (codec == QSV_CODEC_AVC) ? static_cast<mfxI16>(16)
                                                    : static_cast<mfxI16>(32);

    if (pParams->bIntraRefEncoding == true) {
      CO->RecoveryPointSEI = MFX_CODINGOPTION_ON;
    }

    CO->RateDistortionOpt = GetCodingOpt(pParams->bRDO);
    blog(LOG_INFO, "\tRDO set: %s", GetCodingOptStatus(CO->RateDistortionOpt).c_str());

    CO->VuiVclHrdParameters = GetCodingOpt(pParams->bHRDConformance);
    CO->VuiNalHrdParameters = GetCodingOpt(pParams->bHRDConformance);
    CO->NalHrdConformance = GetCodingOpt(pParams->bHRDConformance);
    blog(LOG_INFO, "\tHRDConformance set: %s",
         GetCodingOptStatus(CO->NalHrdConformance).c_str());
  }

  if (mfx_Ext_CO2_enable == 1) {
    auto CO2 = mfx_EncParams.AddExtBuffer<mfxExtCodingOption2>();

    CO2->RepeatPPS = MFX_CODINGOPTION_OFF;

    if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
      CO2->MaxFrameSize = (mfx_EncParams.mfx.TargetKbps *
                           mfx_EncParams.mfx.BRCParamMultiplier * 1000 /
                           (8 * (mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
                                 mfx_EncParams.mfx.FrameInfo.FrameRateExtD))) *
                          10;
    }

    CO2->ExtBRC = GetCodingOpt(pParams->bExtBRC);
    blog(LOG_INFO, "\tExtBRC set: %s", GetCodingOptStatus(CO2->ExtBRC).c_str());

    if (pParams->bIntraRefEncoding == true) {

      CO2->IntRefType = MFX_REFRESH_HORIZONTAL;

      CO2->IntRefCycleSize = static_cast<mfxU16>(
          pParams->nIntraRefCycleSize > 1
              ? pParams->nIntraRefCycleSize
              : (mfx_EncParams.mfx.GopRefDist > 1 ? mfx_EncParams.mfx.GopRefDist
                                                  : 2));
      blog(LOG_INFO, "\tIntraRefCycleSize set: %d", CO2->IntRefCycleSize);
      if (pParams->nIntraRefQPDelta > -52 && pParams->nIntraRefQPDelta < 52) {
        CO2->IntRefQPDelta = static_cast<mfxU16>(pParams->nIntraRefQPDelta);
        blog(LOG_INFO, "\tIntraRefQPDelta set: %d", CO2->IntRefQPDelta);
      }
    }

    if (mfx_Platform.CodeName < MFX_PLATFORM_DG2 &&
        mfx_Platform.MediaAdapterType == MFX_MEDIA_INTEGRATED &&
        pParams->bLookahead == true && pParams->bLowpower == false) {
      pParams->bLookahead = false;
      pParams->nLADepth = 0;
      blog(LOG_INFO, "\tIntegrated graphics with Lowpower turned OFF does not "
                     "\tsupport Lookahead");
    }

    if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
      if (pParams->bLookahead == true) {
        CO2->LookAheadDepth = pParams->nLADepth;
        blog(LOG_INFO, "\tLookahead set to: ON");
        blog(LOG_INFO, "\tLookaheadDepth set to: %d", CO2->LookAheadDepth);
      }
    }

    if (codec != QSV_CODEC_VP9) {
      CO2->MBBRC = GetCodingOpt(pParams->bMBBRC);
      blog(LOG_INFO, "\tMBBRC set: %s", GetCodingOptStatus(CO2->MBBRC).c_str());
    }

    if (pParams->nGOPRefDist > 1) {
      CO2->BRefType = MFX_B_REF_PYRAMID;
      blog(LOG_INFO, "\tBPyramid set: ON");
    } else {
      CO2->BRefType = MFX_B_REF_UNKNOWN;
      blog(LOG_INFO, "\tBPyramid set: AUTO");
    }

    if (pParams->nTrellis.has_value()) {
      switch (pParams->nTrellis.value()) {
      case 0:
        CO2->Trellis = MFX_TRELLIS_OFF;
        blog(LOG_INFO, "\tTrellis set: OFF");
        break;
      case 1:
        CO2->Trellis = MFX_TRELLIS_I;
        blog(LOG_INFO, "\tTrellis set: I");
        break;
      case 2:
        CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P;
        blog(LOG_INFO, "\tTrellis set: IP");
        break;
      case 3:
        CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B;
        blog(LOG_INFO, "\tTrellis set: IPB");
        break;
      case 4:
        CO2->Trellis = MFX_TRELLIS_I | MFX_TRELLIS_B;
        blog(LOG_INFO, "\tTrellis set: IB");
        break;
      case 5:
        CO2->Trellis = MFX_TRELLIS_P;
        blog(LOG_INFO, "\tTrellis set: P");
        break;
      case 6:
        CO2->Trellis = MFX_TRELLIS_P | MFX_TRELLIS_B;
        blog(LOG_INFO, "\tTrellis set: PB");
        break;
      case 7:
        CO2->Trellis = MFX_TRELLIS_B;
        blog(LOG_INFO, "\tTrellis set: B");
        break;
      default:
        blog(LOG_INFO, "\tTrellis set: AUTO");
        break;
      }
    }

    CO2->AdaptiveI = GetCodingOpt(pParams->bAdaptiveI);
    blog(LOG_INFO, "\tAdaptiveI set: %s", GetCodingOptStatus(CO2->AdaptiveI).c_str());

    CO2->AdaptiveB = GetCodingOpt(pParams->bAdaptiveB);
    blog(LOG_INFO, "\tAdaptiveB set: %s", GetCodingOptStatus(CO2->AdaptiveB).c_str());

    if (pParams->RateControl == MFX_RATECONTROL_CBR ||
        pParams->RateControl == MFX_RATECONTROL_VBR) {
      CO2->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
      if (pParams->nLookAheadDS.has_value() == true) {
        switch (pParams->nLookAheadDS.value()) {
        case 0:
          CO2->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
          blog(LOG_INFO, "\tLookAheadDS set: SLOW");
          break;
        case 1:
          CO2->LookAheadDS = MFX_LOOKAHEAD_DS_2x;
          blog(LOG_INFO, "\tLookAheadDS set: MEDIUM");
          break;
        case 2:
          CO2->LookAheadDS = MFX_LOOKAHEAD_DS_4x;
          blog(LOG_INFO, "\tLookAheadDS set: FAST");
          break;
        default:
          blog(LOG_INFO, "\tLookAheadDS set: AUTO");
          break;
        }
      }
    }

    CO2->UseRawRef = GetCodingOpt(pParams->bRawRef);
    blog(LOG_INFO, "\tUseRawRef set: %s", GetCodingOptStatus(CO2->UseRawRef).c_str());
  }

  if (mfx_Ext_CO3_enable == 1) {
    auto CO3 = mfx_EncParams.AddExtBuffer<mfxExtCodingOption3>();
    CO3->TargetBitDepthLuma = pParams->video_fmt_10bit ? 10 : 8;
    CO3->TargetBitDepthChroma = pParams->video_fmt_10bit ? 10 : 8;
    CO3->TargetChromaFormatPlus1 =
        static_cast<mfxU16>(mfx_EncParams.mfx.FrameInfo.ChromaFormat + 1);

    if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
      CO3->MaxFrameSizeI = (mfx_EncParams.mfx.TargetKbps *
                            mfx_EncParams.mfx.BRCParamMultiplier * 1000 /
                            (8 * (mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
                                  mfx_EncParams.mfx.FrameInfo.FrameRateExtD))) *
                           8;
      CO3->MaxFrameSizeP = (mfx_EncParams.mfx.TargetKbps *
                            mfx_EncParams.mfx.BRCParamMultiplier * 1000 /
                            (8 * (mfx_EncParams.mfx.FrameInfo.FrameRateExtN /
                                  mfx_EncParams.mfx.FrameInfo.FrameRateExtD))) *
                           5;
    }

    CO3->MBDisableSkipMap = MFX_CODINGOPTION_ON;
    CO3->EnableQPOffset = MFX_CODINGOPTION_ON;
    CO3->BitstreamRestriction = MFX_CODINGOPTION_ON;

    CO3->WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
    CO3->WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;

    if (pParams->bIntraRefEncoding == true) {
      CO3->IntRefCycleDist = 0;
    }

    CO3->ContentInfo = MFX_CONTENT_NOISY_VIDEO;

    if ((codec == QSV_CODEC_AVC || codec == QSV_CODEC_AV1) &&
        pParams->bLookahead == true) {
      CO3->ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;
      blog(LOG_INFO, "\tScenario: GAME STREAMING");

    } else if (pParams->bLookahead == false ||
               (codec == QSV_CODEC_HEVC && pParams->bLookahead == true)) {
      CO3->ScenarioInfo = MFX_SCENARIO_REMOTE_GAMING;
      blog(LOG_INFO, "\tScenario: REMOTE GAMING");
    }

    if (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
      CO3->EnableMBQP = MFX_CODINGOPTION_ON;
    }

    /*This parameter sets active references for frames. This is fucking
       magic, it may work with LookAhead, or it may not work, it seems to
       depend on the IDE and compiler. If you get frame drops, delete it.*/
    if (codec == QSV_CODEC_HEVC) {
      if (pParams->nNumRefFrameLayers > 0) {

        std::fill(CO3->NumRefActiveP, CO3->NumRefActiveP + 8,
                  (mfxU16)HEVCGetMaxNumRefActivePL0(
                      mfx_EncParams.mfx.TargetUsage, mfx_EncParams.mfx.LowPower,
                      mfx_EncParams.mfx.FrameInfo));
        std::fill(
            CO3->NumRefActiveBL0, CO3->NumRefActiveBL0 + 8,
            (mfxU16)HEVCGetMaxNumRefActiveBL0(mfx_EncParams.mfx.TargetUsage,
                                              mfx_EncParams.mfx.LowPower));
        std::fill(CO3->NumRefActiveBL1, CO3->NumRefActiveBL1 + 8,
                  (mfxU16)HEVCGetMaxNumRefActiveBL1(
                      mfx_EncParams.mfx.TargetUsage, mfx_EncParams.mfx.LowPower,
                      mfx_EncParams.mfx.FrameInfo));

        blog(LOG_INFO, "\tNumRefFrameLayer set: %d", pParams->nNumRefFrame);
      }
    } else if (codec == QSV_CODEC_AV1 || codec == QSV_CODEC_VP9) {
      if (pParams->nNumRefFrameLayers > 0) {
        std::fill(CO3->NumRefActiveP, CO3->NumRefActiveP + 8,
                  mfx_EncParams.mfx.NumRefFrame);
        std::fill(CO3->NumRefActiveBL0, CO3->NumRefActiveBL0 + 8,
                  mfx_EncParams.mfx.NumRefFrame);
        std::fill(CO3->NumRefActiveBL1, CO3->NumRefActiveBL1 + 8,
                  mfx_EncParams.mfx.NumRefFrame);

        blog(LOG_INFO, "\tNumRefFrameLayer set: %d", pParams->nNumRefFrame);
      }
    }

    if (codec == QSV_CODEC_HEVC) {
      CO3->GPB = GetCodingOpt(pParams->bGPB);
      blog(LOG_INFO, "\tGPB set: %s", GetCodingOptStatus(CO3->GPB).c_str());
    }

    if (pParams->bPPyramid == true) {
      CO3->PRefType = MFX_P_REF_PYRAMID;
      blog(LOG_INFO, "\tPPyramid set: PYRAMID");
    } else {
      CO3->PRefType = MFX_P_REF_SIMPLE;
      blog(LOG_INFO, "\tPPyramid set: SIMPLE");
    }

    CO3->AdaptiveCQM = GetCodingOpt(pParams->bAdaptiveCQM);
    blog(LOG_INFO, "\tAdaptiveCQM set: %s",
         GetCodingOptStatus(CO3->AdaptiveCQM).c_str());

    if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 4) {
      CO3->AdaptiveRef = GetCodingOpt(pParams->bAdaptiveRef);
      blog(LOG_INFO, "\tAdaptiveRef set: %s",
           GetCodingOptStatus(CO3->AdaptiveRef).c_str());

      CO3->AdaptiveLTR = GetCodingOpt(pParams->bAdaptiveLTR);
      if (pParams->bExtBRC == true && codec == QSV_CODEC_AVC) {
        CO3->ExtBrcAdaptiveLTR = GetCodingOpt(pParams->bAdaptiveLTR);
      }
      blog(LOG_INFO, "\tAdaptiveLTR set: %s",
           GetCodingOptStatus(CO3->AdaptiveLTR).c_str());
    }

    if (pParams->nWinBRCMaxAvgSize > 0) {
      CO3->WinBRCMaxAvgKbps = static_cast<mfxU16>(pParams->nWinBRCMaxAvgSize);
      blog(LOG_INFO, "\tWinBRCMaxSize set: %d", CO3->WinBRCMaxAvgKbps);
    }

    if (pParams->nWinBRCSize > 0) {
      CO3->WinBRCSize = static_cast<mfxU16>(pParams->nWinBRCSize);
      blog(LOG_INFO, "\tWinBRCSize set: %d", CO3->WinBRCSize);
    }

    CO3->MotionVectorsOverPicBoundaries =
        GetCodingOpt(pParams->nMotionVectorsOverPicBoundaries);
    blog(LOG_INFO, "\tMotionVectorsOverPicBoundaries set: %s",
         GetCodingOptStatus(CO3->MotionVectorsOverPicBoundaries).c_str());

    if (pParams->bGlobalMotionBiasAdjustment.has_value() &&
        pParams->bGlobalMotionBiasAdjustment.value() == true) {
      CO3->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_ON;
      blog(LOG_INFO, "\tGlobalMotionBiasAdjustment set: ON");
      if (pParams->nMVCostScalingFactor.has_value()) {
        switch (pParams->nMVCostScalingFactor.value()) {
        case 1:
          CO3->MVCostScalingFactor = 1;
          blog(LOG_INFO, "\tMVCostScalingFactor set: 1/2");
          break;
        case 2:
          CO3->MVCostScalingFactor = 2;
          blog(LOG_INFO, "\tMVCostScalingFactor set: 1/4");
          break;
        case 3:
          CO3->MVCostScalingFactor = 3;
          blog(LOG_INFO, "\tMVCostScalingFactor set: 1/8");
          break;
        default:
          blog(LOG_INFO, "\tMVCostScalingFactor set: DEFAULT");
          break;
        }
      } else {
        blog(LOG_INFO, "\tMVCostScalingFactor set: AUTO");
      }
    } else {
      CO3->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_OFF;
    }

    CO3->DirectBiasAdjustment = GetCodingOpt(pParams->bDirectBiasAdjustment);
    blog(LOG_INFO, "\tDirectBiasAdjustment set: %s",
         GetCodingOptStatus(CO3->DirectBiasAdjustment).c_str());
  }

#if defined(_WIN32) || defined(_WIN64)
  if (codec != QSV_CODEC_VP9 &&
      (mfx_Version.Major >= 2 && mfx_Version.Minor >= 8) &&
      pParams->bEncTools == true) {
    auto EncToolsParam = mfx_EncParams.AddExtBuffer<mfxExtEncToolsConfig>();
    switch (codec) {
    case QSV_CODEC_AVC:
      EncToolsParam->AdaptiveLTR = MFX_CODINGOPTION_OFF;
      EncToolsParam->BRC = (pParams->RateControl == MFX_RATECONTROL_CBR ||
                            pParams->RateControl == MFX_RATECONTROL_VBR)
                               ? MFX_CODINGOPTION_ON
                               : MFX_CODINGOPTION_UNKNOWN;
      EncToolsParam->AdaptiveLTR = GetCodingOpt(pParams->bAdaptiveLTR);
      EncToolsParam->AdaptiveRefB = GetCodingOpt(pParams->bAdaptiveRef);
      EncToolsParam->AdaptiveRefP = GetCodingOpt(pParams->bAdaptiveRef);
      EncToolsParam->AdaptivePyramidQuantB =
          pParams->nGOPRefDist > 1 ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
      EncToolsParam->AdaptivePyramidQuantP = pParams->bPPyramid == true
                                                 ? MFX_CODINGOPTION_ON
                                                 : MFX_CODINGOPTION_OFF;
      EncToolsParam->BRCBufferHints =
          (pParams->RateControl == MFX_RATECONTROL_CBR ||
           pParams->RateControl == MFX_RATECONTROL_VBR)
              ? MFX_CODINGOPTION_ON
              : MFX_CODINGOPTION_UNKNOWN;
      EncToolsParam->SceneChange = MFX_CODINGOPTION_ON;
      EncToolsParam->AdaptiveMBQP = GetCodingOpt(pParams->bMBBRC);
      EncToolsParam->AdaptiveQuantMatrices = MFX_CODINGOPTION_ON;
      EncToolsParam->AdaptiveB = GetCodingOpt(pParams->bAdaptiveB);
      EncToolsParam->AdaptiveI = GetCodingOpt(pParams->bAdaptiveI);
      EncToolsParam->SaliencyMapHint = MFX_CODINGOPTION_OFF;

      if (pParams->bLookahead == true) {
        EncToolsParam->BRC = MFX_CODINGOPTION_OFF;
      }
      break;
    case QSV_CODEC_AV1:
      EncToolsParam->BRC = (pParams->RateControl == MFX_RATECONTROL_CBR ||
                            pParams->RateControl == MFX_RATECONTROL_VBR)
                               ? MFX_CODINGOPTION_ON
                               : MFX_CODINGOPTION_OFF;
      EncToolsParam->AdaptiveLTR = MFX_CODINGOPTION_OFF;
      EncToolsParam->AdaptiveRefB = GetCodingOpt(pParams->bAdaptiveRef);
      EncToolsParam->AdaptiveRefP = GetCodingOpt(pParams->bAdaptiveRef);
      EncToolsParam->AdaptivePyramidQuantB =
          pParams->nGOPRefDist > 1 ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
      EncToolsParam->AdaptivePyramidQuantP = pParams->bPPyramid == true
                                                 ? MFX_CODINGOPTION_ON
                                                 : MFX_CODINGOPTION_OFF;
      EncToolsParam->BRCBufferHints = EncToolsParam->BRC;
      EncToolsParam->SceneChange = MFX_CODINGOPTION_ON;
      EncToolsParam->AdaptiveMBQP = GetCodingOpt(pParams->bMBBRC);
      EncToolsParam->AdaptiveQuantMatrices =
          GetCodingOpt(pParams->bAdaptiveCQM);
      EncToolsParam->AdaptiveB = GetCodingOpt(pParams->bAdaptiveB);
      EncToolsParam->AdaptiveI = GetCodingOpt(pParams->bAdaptiveI);
      EncToolsParam->SaliencyMapHint = MFX_CODINGOPTION_ON;
      break;
    case QSV_CODEC_HEVC:
      EncToolsParam->BRC = (pParams->RateControl == MFX_RATECONTROL_CBR ||
                            pParams->RateControl == MFX_RATECONTROL_VBR)
                               ? MFX_CODINGOPTION_ON
                               : MFX_CODINGOPTION_UNKNOWN;
      EncToolsParam->AdaptiveLTR = GetCodingOpt(pParams->bAdaptiveLTR);
      EncToolsParam->AdaptiveRefB = GetCodingOpt(pParams->bAdaptiveRef);
      EncToolsParam->AdaptiveRefP = GetCodingOpt(pParams->bAdaptiveRef);
      EncToolsParam->AdaptivePyramidQuantB =
          pParams->nGOPRefDist > 1 ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
      EncToolsParam->AdaptivePyramidQuantP = pParams->bPPyramid == true
                                                 ? MFX_CODINGOPTION_ON
                                                 : MFX_CODINGOPTION_OFF;
      EncToolsParam->BRCBufferHints =
          (pParams->RateControl == MFX_RATECONTROL_CBR ||
           pParams->RateControl == MFX_RATECONTROL_VBR)
              ? MFX_CODINGOPTION_ON
              : MFX_CODINGOPTION_UNKNOWN;
      EncToolsParam->SceneChange = MFX_CODINGOPTION_ON;
      EncToolsParam->AdaptiveMBQP = GetCodingOpt(pParams->bMBBRC);
      EncToolsParam->AdaptiveQuantMatrices =
          GetCodingOpt(pParams->bAdaptiveCQM);
      EncToolsParam->AdaptiveB = GetCodingOpt(pParams->bAdaptiveB);
      EncToolsParam->AdaptiveI = GetCodingOpt(pParams->bAdaptiveI);
      EncToolsParam->SaliencyMapHint = MFX_CODINGOPTION_ON;

      if (pParams->bLookahead == true) {
        EncToolsParam->BRC = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptiveLTR = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptiveRefB = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptiveRefP = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptivePyramidQuantB = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptivePyramidQuantP = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->BRCBufferHints = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->SceneChange = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptiveQuantMatrices = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptiveB = MFX_CODINGOPTION_UNKNOWN;
        EncToolsParam->AdaptiveI = MFX_CODINGOPTION_UNKNOWN;
      }

      if (pParams->bLookahead == true) {
        EncToolsParam->BRC = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptiveLTR = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptiveRefB = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptiveRefP = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptivePyramidQuantB = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptivePyramidQuantP = MFX_CODINGOPTION_OFF;
        EncToolsParam->BRCBufferHints = MFX_CODINGOPTION_OFF;
        EncToolsParam->SceneChange = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptiveQuantMatrices = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptiveB = MFX_CODINGOPTION_OFF;
        EncToolsParam->AdaptiveI = MFX_CODINGOPTION_OFF;
      }
      break;
    }
    blog(LOG_INFO, "\tEncTools set: ON");
  }

  /*Don't touch it! Magic beyond the control of mere mortals takes place
   * here*/
  if (mfx_Ext_CO_DDI_enable == 1 && codec != QSV_CODEC_AV1) {
    auto CODDI = mfx_EncParams.AddExtBuffer<mfxExtCodingOptionDDI>();
    CODDI->WriteIVFHeaders = MFX_CODINGOPTION_OFF;
    CODDI->IBC = MFX_CODINGOPTION_ON;
    CODDI->BRCPrecision = 3;
    CODDI->BiDirSearch = MFX_CODINGOPTION_ON;
    CODDI->DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;
    CODDI->GlobalSearch = 1;
    CODDI->IntraPredCostType = 8;
    CODDI->MEFractionalSearchType = 16;
    CODDI->MVPrediction = MFX_CODINGOPTION_ON;
    CODDI->MaxMVs =
        static_cast<mfxU16>((pParams->nWidth * pParams->nHeight) /* / 4*/);
    CODDI->WeightedBiPredIdc = 2;
    CODDI->WeightedPrediction = MFX_CODINGOPTION_ON;
    CODDI->FieldPrediction = MFX_CODINGOPTION_ON;
    // CO_DDI->CabacInitIdcPlus1 = (mfxU16)1;
    CODDI->DirectCheck = MFX_CODINGOPTION_ON;
    CODDI->FractionalQP = 1;
    CODDI->Hme = MFX_CODINGOPTION_ON;
    CODDI->LocalSearch = 6;
    CODDI->MBAFF = MFX_CODINGOPTION_ON;
    CODDI->DDI.InterPredBlockSize = 64;
    CODDI->DDI.IntraPredBlockSize = 1;
    CODDI->RefOppositeField = MFX_CODINGOPTION_ON;
    CODDI->RefRaw = GetCodingOpt(pParams->bRawRef);
    CODDI->TMVP = MFX_CODINGOPTION_ON;
    CODDI->DisablePSubMBPartition = MFX_CODINGOPTION_OFF;
    CODDI->DisableBSubMBPartition = MFX_CODINGOPTION_OFF;
    CODDI->QpAdjust = MFX_CODINGOPTION_ON;
    CODDI->Transform8x8Mode = MFX_CODINGOPTION_ON;
    CODDI->EarlySkip = 0;
    CODDI->RefreshFrameContext = MFX_CODINGOPTION_ON;
    CODDI->ChangeFrameContextIdxForTS = MFX_CODINGOPTION_ON;
    CODDI->SuperFrameForTS = MFX_CODINGOPTION_ON;

    if (codec == QSV_CODEC_HEVC) {
      CODDI->NumActiveRefP = static_cast<mfxU16>(HEVCGetMaxNumRefActivePL0(
          mfx_EncParams.mfx.TargetUsage, mfx_EncParams.mfx.LowPower,
          mfx_EncParams.mfx.FrameInfo));
      CODDI->NumActiveRefBL0 = static_cast<mfxU16>(HEVCGetMaxNumRefActiveBL0(
          mfx_EncParams.mfx.TargetUsage, mfx_EncParams.mfx.LowPower));
      CODDI->NumActiveRefBL1 = static_cast<mfxU16>(HEVCGetMaxNumRefActiveBL1(
          mfx_EncParams.mfx.TargetUsage, mfx_EncParams.mfx.LowPower,
          mfx_EncParams.mfx.FrameInfo));
    } else if (codec == QSV_CODEC_VP9) {
      CODDI->NumActiveRefP = static_cast<mfxU16>(pParams->nNumRefFrame);
      CODDI->NumActiveRefBL0 = static_cast<mfxU16>(pParams->nNumRefFrame);
      CODDI->NumActiveRefBL1 = static_cast<mfxU16>(pParams->nNumRefFrame);
    }
    /*You can touch it, this is the LookAHead setting,
            here you can adjust its strength,
            range of quality and dependence.*/
    if (pParams->bLookahead == true && pParams->nLADepth >= 10) {
      CODDI->StrengthN = 1000;
      CODDI->QpUpdateRange = 30;
      CODDI->LaScaleFactor = 1;
      CODDI->LookAheadDependency = static_cast<mfxU16>(pParams->nLADepth - 10);
    }
  }
#endif

  if (codec == QSV_CODEC_HEVC) {
    auto HevcParam = mfx_EncParams.AddExtBuffer<mfxExtHEVCParam>();

    auto ChromaLocParam = mfx_EncParams.AddExtBuffer<mfxExtChromaLocInfo>();

    auto HevcTilesParam = mfx_EncParams.AddExtBuffer<mfxExtHEVCTiles>();

    ChromaLocParam->ChromaLocInfoPresentFlag = 1;
    ChromaLocParam->ChromaSampleLocTypeTopField =
        static_cast<mfxU16>(pParams->ChromaSampleLocTypeTopField);
    ChromaLocParam->ChromaSampleLocTypeBottomField =
        static_cast<mfxU16>(pParams->ChromaSampleLocTypeBottomField);

    HevcParam->PicWidthInLumaSamples = mfx_EncParams.mfx.FrameInfo.Width;
    HevcParam->PicHeightInLumaSamples = mfx_EncParams.mfx.FrameInfo.Height;
    /*	HevcParam->LCUSize = pParams->nCTU;*/
    if (pParams->nSAO.has_value()) {
      switch (pParams->nSAO.value()) {
      case 0:
        HevcParam->SampleAdaptiveOffset = MFX_SAO_DISABLE;
        blog(LOG_INFO, "\tSAO set: DISABLE");
        break;
      case 1:
        HevcParam->SampleAdaptiveOffset = MFX_SAO_ENABLE_LUMA;
        blog(LOG_INFO, "\tSAO set: LUMA");
        break;
      case 2:
        HevcParam->SampleAdaptiveOffset = MFX_SAO_ENABLE_CHROMA;
        blog(LOG_INFO, "\tSAO set: CHROMA");
        break;
      case 3:
        HevcParam->SampleAdaptiveOffset =
            MFX_SAO_ENABLE_LUMA | MFX_SAO_ENABLE_CHROMA;
        blog(LOG_INFO, "\tSAO set: ALL");
        break;
      }
    } else {
      blog(LOG_INFO, "\tDirectBiasAdjustment set: AUTO");
    }
    HevcTilesParam->NumTileColumns = 1;
    HevcTilesParam->NumTileRows = 1;
  }

  if (codec == QSV_CODEC_AVC &&
      (mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
       mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
       mfx_EncParams.mfx.RateControlMethod == MFX_RATECONTROL_CQP) &&
      pParams->nDenoiseMode > -1) {
    auto DenoiseParam = mfx_EncParams.AddExtBuffer<mfxExtVPPDenoise2>();
    if (pParams->nDenoiseMode.has_value()) {
      switch (pParams->nDenoiseMode.value()) {
      case 1:
        DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE;
        blog(LOG_INFO, "\tDenoise set: AUTO | BDRATE | PRE ENCODE");
        break;
      case 2:
        DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST;
        blog(LOG_INFO, "\tDenoise set: AUTO | ADJUST | POST ENCODE");
        break;
      case 3:
        DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE;
        blog(LOG_INFO, "\tDenoise set: AUTO | SUBJECTIVE | PRE ENCODE");
        break;
      case 4:
        DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL;
        DenoiseParam->Strength = static_cast<mfxU16>(pParams->nDenoiseStrength);
        blog(LOG_INFO, "\tDenoise set: MANUAL | STRENGTH %d | PRE ENCODE",
             DenoiseParam->Strength);
        break;
      case 5:
        DenoiseParam->Mode = MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL;
        DenoiseParam->Strength = static_cast<mfxU16>(pParams->nDenoiseStrength);
        blog(LOG_INFO, "\tDenoise set: MANUAL | STRENGTH %d | POST ENCODE",
             DenoiseParam->Strength);
        break;
      default:
        DenoiseParam->Mode = MFX_DENOISE_MODE_DEFAULT;
        blog(LOG_INFO, "\tDenoise set: DEFAULT");
        break;
      }
    } else {
      blog(LOG_INFO, "\tDenoise set: OFF");
    }
  }

  if (codec == QSV_CODEC_AV1) {
    if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 5) {
      auto AV1BitstreamParam =
          mfx_EncParams.AddExtBuffer<mfxExtAV1BitstreamParam>();

      auto AV1TileParam = mfx_EncParams.AddExtBuffer<mfxExtAV1TileParam>();

      AV1BitstreamParam->WriteIVFHeaders = MFX_CODINGOPTION_OFF;

      AV1TileParam->NumTileGroups = 1;
      if ((pParams->nHeight * pParams->nWidth) >= 8294400) {
        AV1TileParam->NumTileColumns = 2;
        AV1TileParam->NumTileRows = 2;
      } else {
        AV1TileParam->NumTileColumns = 1;
        AV1TileParam->NumTileRows = 1;
      }
    }

    if (mfx_Version.Major >= 2 && mfx_Version.Minor >= 9 &&
        pParams->nTuneQualityMode.has_value()) {
      auto TuneQuality = mfx_EncParams.AddExtBuffer<mfxExtTuneEncodeQuality>();

      switch ((int)pParams->nTuneQualityMode.value()) {
      default:
      case 0:
        TuneQuality->TuneQuality = MFX_ENCODE_TUNE_OFF;
        blog(LOG_INFO, "\tTuneQualityMode set: DEFAULT");
        break;
      case 1:
        TuneQuality->TuneQuality = MFX_ENCODE_TUNE_PSNR;
        blog(LOG_INFO, "\tTuneQualityMode set: PSNR");
        break;
      case 2:
        TuneQuality->TuneQuality = MFX_ENCODE_TUNE_SSIM;
        blog(LOG_INFO, "\tTuneQualityMode set: SSIM");
        break;
      case 3:
        TuneQuality->TuneQuality = MFX_ENCODE_TUNE_MS_SSIM;
        blog(LOG_INFO, "\tTuneQualityMode set: MS SSIM");
        break;
      case 4:
        TuneQuality->TuneQuality = MFX_ENCODE_TUNE_VMAF;
        blog(LOG_INFO, "\tTuneQualityMode set: VMAF");
        break;
      case 5:
        TuneQuality->TuneQuality = MFX_ENCODE_TUNE_PERCEPTUAL;
        blog(LOG_INFO, "\tTuneQualityMode set: PERCEPTUAL");
        break;
      }
    } else {
      blog(LOG_INFO, "\tTuneQualityMode set: OFF");
    }
  }

  if (codec == QSV_CODEC_VP9) {
    auto VP9Param = mfx_EncParams.AddExtBuffer<mfxExtVP9Param>();

    VP9Param->WriteIVFHeaders = MFX_CODINGOPTION_OFF;

    if ((pParams->nHeight * pParams->nWidth) >= 8294400) {
      VP9Param->NumTileColumns = 2;
      VP9Param->NumTileRows = 2;
    } else {
      VP9Param->NumTileColumns = 1;
      VP9Param->NumTileRows = 1;
    }

  }
#if defined(_WIN32) || defined(_WIN64)
  else {
    auto VideoSignalParam = mfx_EncParams.AddExtBuffer<mfxExtVideoSignalInfo>();

    VideoSignalParam->VideoFormat = static_cast<mfxU16>(pParams->VideoFormat);
    VideoSignalParam->VideoFullRange =
        static_cast<mfxU16>(pParams->VideoFullRange);
    VideoSignalParam->ColourDescriptionPresent = 1;
    VideoSignalParam->ColourPrimaries =
        static_cast<mfxU16>(pParams->ColourPrimaries);
    VideoSignalParam->TransferCharacteristics =
        static_cast<mfxU16>(pParams->TransferCharacteristics);
    VideoSignalParam->MatrixCoefficients =
        static_cast<mfxU16>(pParams->MatrixCoefficients);
  }
#endif

  if ((codec == QSV_CODEC_HEVC ||
       ((codec == QSV_CODEC_AV1 || codec == QSV_CODEC_VP9) &&
        (mfx_Version.Major >= 2 && mfx_Version.Minor >= 9))) &&
      pParams->MaxContentLightLevel > 0) {
    auto ColourVolumeParam =
        mfx_EncParams.AddExtBuffer<mfxExtMasteringDisplayColourVolume>();
    auto ContentLightLevelParam =
        mfx_EncParams.AddExtBuffer<mfxExtContentLightLevelInfo>();

    ColourVolumeParam->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    ColourVolumeParam->DisplayPrimariesX[0] =
        static_cast<mfxU16>(pParams->DisplayPrimariesX[0]);
    ColourVolumeParam->DisplayPrimariesX[1] =
        static_cast<mfxU16>(pParams->DisplayPrimariesX[1]);
    ColourVolumeParam->DisplayPrimariesX[2] =
        static_cast<mfxU16>(pParams->DisplayPrimariesX[2]);
    ColourVolumeParam->DisplayPrimariesY[0] =
        static_cast<mfxU16>(pParams->DisplayPrimariesY[0]);
    ColourVolumeParam->DisplayPrimariesY[1] =
        static_cast<mfxU16>(pParams->DisplayPrimariesY[1]);
    ColourVolumeParam->DisplayPrimariesY[2] =
        static_cast<mfxU16>(pParams->DisplayPrimariesY[2]);
    ColourVolumeParam->WhitePointX = static_cast<mfxU16>(pParams->WhitePointX);
    ColourVolumeParam->WhitePointY = static_cast<mfxU16>(pParams->WhitePointY);
    ColourVolumeParam->MaxDisplayMasteringLuminance =
        static_cast<mfxU16>(pParams->MaxDisplayMasteringLuminance);
    ColourVolumeParam->MinDisplayMasteringLuminance =
        static_cast<mfxU16>(pParams->MinDisplayMasteringLuminance);

    ContentLightLevelParam->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    ContentLightLevelParam->MaxContentLightLevel =
        static_cast<mfxU16>(pParams->MaxContentLightLevel);
    ContentLightLevelParam->MaxPicAverageLightLevel =
        static_cast<mfxU16>(pParams->MaxPicAverageLightLevel);
  }

  mfx_EncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

  if (codec == QSV_CODEC_AVC && pParams->nDenoiseMode > -1) {
    mfx_EncParams.IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  }

  blog(LOG_INFO, "Feature extended buffer size: %d", mfx_EncParams.NumExtParam);

  // We dont check what was valid or invalid here, just try changing lower
  // power. Ensure set values are not overwritten so in case it wasnt lower
  // power we fail during the parameter check.
  mfxVideoParam validParams = {0};
  memcpy(&validParams, &mfx_EncParams, sizeof(mfxVideoParam));
  mfxStatus sts = mfx_VideoENC->Query(&mfx_EncParams, &validParams);
  if (sts == MFX_ERR_UNSUPPORTED || sts == MFX_ERR_UNDEFINED_BEHAVIOR) {
    auto CO3 = mfx_EncParams.GetExtBuffer<mfxExtCodingOption3>();
    if (CO3->AdaptiveLTR == MFX_CODINGOPTION_ON) {
      CO3->AdaptiveLTR = MFX_CODINGOPTION_OFF;
    }
  }
  return sts;
}

bool QSV_VPL_Encoder_Internal::UpdateParams(struct qsv_param_t *pParams) {
  ResetParamChanged = false;

  mfxStatus sts = mfx_VideoENC->GetVideoParam(&mfx_ResetParams);

  mfx_ResetParams.NumExtParam = 0;
  switch (pParams->RateControl) {
  case MFX_RATECONTROL_CBR:
    if (mfx_ResetParams.mfx.TargetKbps != pParams->nTargetBitRate) {
      mfx_ResetParams.mfx.TargetKbps =
          static_cast<mfxU16>(pParams->nTargetBitRate);
      ResetParamChanged = true;
    }
    break;
  case MFX_RATECONTROL_VBR:
    if (mfx_ResetParams.mfx.TargetKbps != pParams->nTargetBitRate) {
      mfx_ResetParams.mfx.TargetKbps =
          static_cast<mfxU16>(pParams->nTargetBitRate);
      ResetParamChanged = true;
    }
    if (mfx_ResetParams.mfx.MaxKbps != pParams->nMaxBitRate) {
      mfx_ResetParams.mfx.MaxKbps = static_cast<mfxU16>(pParams->nMaxBitRate);
      ResetParamChanged = true;
    }
    if (mfx_ResetParams.mfx.MaxKbps < mfx_ResetParams.mfx.TargetKbps) {
      mfx_ResetParams.mfx.MaxKbps = mfx_ResetParams.mfx.TargetKbps;
      ResetParamChanged = true;
    }
    break;
  case MFX_RATECONTROL_CQP:
    if (mfx_ResetParams.mfx.QPI != pParams->nQPI) {
      mfx_ResetParams.mfx.QPI = static_cast<mfxU16>(pParams->nQPI);
      mfx_ResetParams.mfx.QPB = static_cast<mfxU16>(pParams->nQPB);
      mfx_ResetParams.mfx.QPP = static_cast<mfxU16>(pParams->nQPP);
      ResetParamChanged = true;
    }
    break;
  case MFX_RATECONTROL_ICQ:
    if (mfx_ResetParams.mfx.ICQQuality != pParams->nICQQuality) {
      mfx_ResetParams.mfx.ICQQuality =
          static_cast<mfxU16>(pParams->nICQQuality);
      ResetParamChanged = true;
    }
  }
  if (ResetParamChanged == true) {
    auto ResetParams = mfx_EncParams.AddExtBuffer<mfxExtEncoderResetOption>();
    ResetParams->StartNewSequence = MFX_CODINGOPTION_ON;
    mfx_VideoENC->Query(&mfx_ResetParams, &mfx_ResetParams);
    return true;
  } else {
    return false;
  }
}

mfxStatus QSV_VPL_Encoder_Internal::ReconfigureEncoder() {
  if (ResetParamChanged == true) {
    return mfx_VideoENC->Reset(&mfx_ResetParams);
  } else {
    return MFX_ERR_NONE;
  }
}

mfxStatus QSV_VPL_Encoder_Internal::GetVideoParam(enum qsv_codec codec) {
  if (codec != QSV_CODEC_VP9) {
    auto SPSPPSParams = mfx_EncParams.AddExtBuffer<mfxExtCodingOptionSPSPPS>();
    SPSPPSParams->SPSBuffer = SPS_Buffer;
    SPSPPSParams->PPSBuffer = PPS_Buffer;
    SPSPPSParams->SPSBufSize = 512;
    SPSPPSParams->PPSBufSize = 128;
  }
  if (codec == QSV_CODEC_HEVC) {
    auto VPSParams = mfx_EncParams.AddExtBuffer<mfxExtCodingOptionVPS>();

    VPSParams->VPSBuffer = VPS_Buffer;
    VPSParams->VPSBufSize = 128;
  }

  mfxStatus sts = mfx_VideoENC->GetVideoParam(&mfx_EncParams);

  blog(LOG_INFO, "\tGetVideoParam status:     %d", sts);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "GetVideoParam error: %d", sts);
    return sts;
  }

  if (codec == QSV_CODEC_HEVC) {
    auto VPSParams = mfx_EncParams.GetExtBuffer<mfxExtCodingOptionVPS>();
    VPS_BufferSize = VPSParams->VPSBufSize;
  }

  if (codec != QSV_CODEC_VP9) {
    auto SPSPPSParams = mfx_EncParams.GetExtBuffer<mfxExtCodingOptionSPSPPS>();
    SPS_BufferSize = SPSPPSParams->SPSBufSize;
    PPS_BufferSize = SPSPPSParams->PPSBufSize;
  }

  return sts;
}

void QSV_VPL_Encoder_Internal::GetVPSSPSPPS(mfxU8 **pVPSBuf, mfxU8 **pSPSBuf,
                                            mfxU8 **pPPSBuf, mfxU16 *pnVPSBuf,
                                            mfxU16 *pnSPSBuf,
                                            mfxU16 *pnPPSBuf) {
  *pVPSBuf = VPS_Buffer;
  *pnVPSBuf = VPS_BufferSize;

  *pSPSBuf = SPS_Buffer;
  *pnSPSBuf = SPS_BufferSize;

  *pPPSBuf = PPS_Buffer;
  *pnPPSBuf = PPS_BufferSize;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadP010(mfxFrameSurface1 *pSurface,
                                             uint8_t *pDataY, uint8_t *pDataUV,
                                             uint32_t strideY,
                                             uint32_t strideUV) {
  mfxU16 w, h, i, pitch;
  mfxU8 *ptr;
  mfxFrameInfo *pInfo = &pSurface->Info;
  mfxFrameData *pData = &pSurface->Data;

  if (pInfo->CropH > 0 && pInfo->CropW > 0) {
    w = pInfo->CropW;
    h = pInfo->CropH;
  } else {
    w = pInfo->Width;
    h = pInfo->Height;
  }

  pitch = pData->Pitch;
  ptr = static_cast<mfxU8 *>(
      (pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch));
  const size_t line_size = (size_t)w * 2;

  // load Y plane
  for (i = 0; i < h; i++)
    memcpy(ptr + i * pitch, pDataY + i * strideY, line_size);

  // load UV plane
  h /= 2;
  ptr = static_cast<mfxU8 *>(
      (pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch));

  for (i = 0; i < h; i++)
    memcpy(ptr + i * pitch, pDataUV + i * strideUV, line_size);

  return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadNV12(mfxFrameSurface1 *pSurface,
                                             uint8_t *pDataY, uint8_t *pDataUV,
                                             uint32_t strideY,
                                             uint32_t strideUV) {
  mfxU16 w, h, i, pitch;
  mfxU8 *ptr;
  mfxFrameInfo *pInfo = &pSurface->Info;
  mfxFrameData *pData = &pSurface->Data;

  if (pInfo->CropH > 0 && pInfo->CropW > 0) {
    w = pInfo->CropW;
    h = pInfo->CropH;
  } else {
    w = pInfo->Width;
    h = pInfo->Height;
  }

  pitch = pData->Pitch;
  ptr = static_cast<mfxU8 *>(
      (pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch));

  // load Y plane
  for (i = 0; i < h; i++)
    memcpy(ptr + i * pitch, pDataY + i * strideY, w);

  // load UV plane
  h /= 2;
  ptr = static_cast<mfxU8 *>(
      (pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch));

  for (i = 0; i < h; i++)
    memcpy(ptr + i * pitch, pDataUV + i * strideUV, w);

  return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::LoadBGRA(mfxFrameSurface1 *pSurface,
                                             uint8_t *pDataY,
                                             uint32_t strideY) {
  mfxU16 w, h, i, pitch;
  mfxU8 *ptr;
  mfxFrameInfo *pInfo = &pSurface->Info;
  mfxFrameData *pData = &pSurface->Data;
  if (pInfo->CropH > 0 && pInfo->CropW > 0) {
    w = pInfo->CropW;
    h = pInfo->CropH;
  } else {
    w = pInfo->Width;
    h = pInfo->Height;
  }

  pitch = pData->Pitch;
  ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;

  // load Y plane
  for (i = 0; i < h; i++)
    memcpy(ptr + i * pitch, pDataY + i * strideY, w);

  ptr = static_cast<mfxU8 *>(
      (pData->B + pInfo->CropX + pInfo->CropY * pData->Pitch));

  for (i = 0; i < h; i++) {
    memcpy(ptr + i * pitch, pDataY + i * strideY, pitch);
  }

  return MFX_ERR_NONE;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode_tex(mfxU64 ts, uint32_t tex_handle,
                                               uint64_t lock_key,
                                               uint64_t *next_key,
                                               mfxBitstream **pBS) {
  mfxStatus sts, sync_sts, release_sts, expand_buffer_sts = MFX_ERR_NONE;
  *pBS = nullptr;
  /*We get a fully valid Surface for encoding the frame. Yes brother is the
   * magic of oneVPL.*/
  sts = mfx_VideoENC->GetSurface(&mfx_EncodeSurface);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Encode.GetSurface error: %d", sts);
    return sts;
  }

  // sts = D3D11Device->simple_copytex(mfx_DX_Handle,
  //                                   (mfxMemId
  //                                   *)mfx_EncodeSurface->Data.MemId,
  //                                   tex_handle, lock_key, (mfxU64
  //                                   *)next_key);
  mfx_EncodeSurface->Data.TimeStamp = ts;

  auto TryCounter = 0;
  /*Encode a frame asynchronously (returns immediately)*/
  do {
    ++TryCounter;
    if (TryCounter > 10) {
      TryCounter = 0;
      break;
    }

    sts = mfx_VideoENC->EncodeFrameAsync(nullptr, mfx_EncodeSurface,
                                         mfx_Bitstream.Get(), &mfx_SyncPoint);

    release_sts = mfx_EncodeSurface->FrameInterface->Release(mfx_EncodeSurface);
    if (release_sts < MFX_ERR_NONE) {
      blog(LOG_WARNING, "Surface release error: %d", release_sts);
    }
#if 0
		blog(LOG_INFO, "----");
		blog(LOG_WARNING, "Encode STS: %d", sts);
#endif
    switch (sts) {
    [[likely]] case MFX_ERR_NONE:
      if ((!!mfx_SyncPoint && (mfx_SyncPoint != nullptr))) {
        do {
          sync_sts = MFXVideoCORE_SyncOperation(mfx_Session, mfx_SyncPoint,
                                                static_cast<mfxU32>(INFINITE));
        } while (sync_sts == MFX_WRN_IN_EXECUTION);

        switch (sync_sts) {
        [[likely]] case MFX_ERR_NONE:
          *pBS = mfx_Bitstream.Get();
          mfx_BufferedSyncPoint = mfx_SyncPoint;
          memset(&mfx_Bitstream.Get()->Data, 0, 0);
          break;
        default:
#if 0
          blog(LOG_WARNING,
               "The frame was processed, but a synchronization error "
               "occurred. \nEncode.Sync error: %d",
               sync_sts);
#endif
          if (!!mfx_BufferedSyncPoint && (mfx_BufferedSyncPoint != nullptr)) {
            /*If it was not possible to synchronize the frame,
            we try to synchronize with the last successful SyncPoint.*/
            do {
              sync_sts =
                  MFXVideoCORE_SyncOperation(mfx_Session, mfx_BufferedSyncPoint,
                                             static_cast<mfxU32>(INFINITE));
            } while (sync_sts == MFX_WRN_IN_EXECUTION);
          }
          break;
        }
      }
      break;
    case MFX_WRN_DEVICE_BUSY:
#if 0
      blog(LOG_WARNING, "Device busy");
#endif
      if (!!mfx_BufferedSyncPoint && (mfx_BufferedSyncPoint != nullptr))
          [[likely]] {
        /*If the device is Busy,
        we try to synchronize with the last successful SyncPoint.*/
        do {
          sync_sts =
              MFXVideoCORE_SyncOperation(mfx_Session, mfx_BufferedSyncPoint,
                                         static_cast<mfxU32>(INFINITE));
        } while (sync_sts == MFX_WRN_IN_EXECUTION);
      } else {
        /*Or, if there is no buffered successful SyncPoint, then we sleep*/
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
      break;
    case MFX_ERR_MORE_BITSTREAM:
    case MFX_ERR_NOT_ENOUGH_BUFFER:
#if 0 
      blog(LOG_WARNING,
           "It is necessary to expand the buffer. BufferSize: %d. Try: %d",
           mfx_Bitstream.BufferSize(), TryCounter);
#endif
      /*If the size of the allocated buffer is insufficient,
      then we expand the buffer size by 10 % per cycle*/
      expand_buffer_sts = mfx_Bitstream.ExpandBuffer();
#if 0
      if (expand_buffer_sts != MFX_ERR_NONE) {
        blog(LOG_WARNING, "Buffer expansion failed: %d", expand_buffer_sts);
      } else {
        blog(LOG_WARNING, "New BufferSize: %d", mfx_Bitstream.BufferSize());
      }
#endif
      break;
    case MFX_ERR_GPU_HANG:
      blog(LOG_WARNING, "GPU HANG");
      break;
    /*The buffer is being filled.The error is ignored.*/
    case MFX_ERR_MORE_DATA:
      break;
    default:
#if 0
      blog(LOG_WARNING, "Encode error: %d", sts);
#endif
      break;
    }
  } while (sts == MFX_WRN_IN_EXECUTION || sts == MFX_WRN_DEVICE_BUSY ||
           sts == MFX_ERR_MORE_BITSTREAM || sts == MFX_ERR_NOT_ENOUGH_BUFFER);
  /*Reset Sync Point to use in the next frame*/
  mfx_SyncPoint = nullptr;

  // memset(pBS, 0, 0);
  // pBS = nullptr;

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Encode(mfxU64 ts, uint8_t *pDataY,
                                           uint8_t *pDataUV, uint32_t strideY,
                                           uint32_t strideUV,
                                           mfxBitstream **pBS) {
  mfxStatus sts, sync_sts, release_sts, expand_buffer_sts = MFX_ERR_NONE;
  *pBS = nullptr;
  /*We get a fully valid Surface for encoding the frame. Yes brother is the
   * magic of oneVPL.*/
  sts = mfx_VideoENC->GetSurface(&mfx_EncodeSurface);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Encode.GetSurface error: %d", sts);
    return sts;
  }

  sts =
      mfx_EncodeSurface->FrameInterface->Map(mfx_EncodeSurface, MFX_MAP_WRITE);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Encode.Surface.Map.Write error: %d", sts);
    return sts;
  }
  sts = (mfx_EncodeSurface->Info.FourCC == MFX_FOURCC_P010)
            ? LoadP010(mfx_EncodeSurface, pDataY, pDataUV, strideY, strideUV)
        : (mfx_EncodeSurface->Info.FourCC == MFX_FOURCC_NV12)
            ? LoadNV12(mfx_EncodeSurface, pDataY, pDataUV, strideY, strideUV)
            : LoadBGRA(mfx_EncodeSurface, pDataY, strideY);
  mfx_EncodeSurface->Data.TimeStamp = ts;
  sts = mfx_EncodeSurface->FrameInterface->Unmap(mfx_EncodeSurface);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Encode.Surface.Unmap.Write error: %d", sts);
    return sts;
  }

  auto TryCounter = 0;
  /*Encode a frame asynchronously (returns immediately)*/
  do {
    ++TryCounter;
    if (TryCounter > 10) {
      TryCounter = 0;
      break;
    }

    sts = mfx_VideoENC->EncodeFrameAsync(nullptr, mfx_EncodeSurface,
                                         mfx_Bitstream.Get(), &mfx_SyncPoint);
#if 0
		blog(LOG_INFO, "----");
		blog(LOG_WARNING, "Encode STS: %d", sts);
#endif
    switch (sts) {
    [[likely]] case MFX_ERR_NONE:
      if ((!!mfx_SyncPoint && (mfx_SyncPoint != nullptr))) {
        do {
          sync_sts = MFXVideoCORE_SyncOperation(mfx_Session, mfx_SyncPoint,
                                                static_cast<mfxU32>(INFINITE));
        } while (sync_sts == MFX_WRN_IN_EXECUTION);

        switch (sync_sts) {
        [[likely]] case MFX_ERR_NONE:
          *pBS = mfx_Bitstream.Get();
          mfx_BufferedSyncPoint = mfx_SyncPoint;
          memset(&mfx_Bitstream.Get()->Data, 0, 0);
          break;
        default:
#if 0
          blog(LOG_WARNING,
               "The frame was processed, but a synchronization error "
               "occurred. \nEncode.Sync error: %d",
               sync_sts);
#endif
          if (!!mfx_BufferedSyncPoint && (mfx_BufferedSyncPoint != nullptr)) {
            /*If it was not possible to synchronize the frame,
            we try to synchronize with the last successful SyncPoint.*/
            do {
              sync_sts =
                  MFXVideoCORE_SyncOperation(mfx_Session, mfx_BufferedSyncPoint,
                                             static_cast<mfxU32>(INFINITE));
            } while (sync_sts == MFX_WRN_IN_EXECUTION);
          }
          break;
        }
      }
      break;
    case MFX_WRN_DEVICE_BUSY:
#if 0
      blog(LOG_WARNING, "Device busy");
#endif
      if (!!mfx_BufferedSyncPoint && (mfx_BufferedSyncPoint != nullptr))
          [[likely]] {
        /*If the device is Busy,
        we try to synchronize with the last successful SyncPoint.*/
        do {
          sync_sts =
              MFXVideoCORE_SyncOperation(mfx_Session, mfx_BufferedSyncPoint,
                                         static_cast<mfxU32>(INFINITE));
        } while (sync_sts == MFX_WRN_IN_EXECUTION);
      } else {
        /*Or, if there is no buffered successful SyncPoint, then we sleep*/
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
      break;
    case MFX_ERR_MORE_BITSTREAM:
    case MFX_ERR_NOT_ENOUGH_BUFFER:
#if 0 
      blog(LOG_WARNING,
           "It is necessary to expand the buffer. BufferSize: %d. Try: %d",
           mfx_Bitstream.BufferSize(), TryCounter);
#endif
      /*If the size of the allocated buffer is insufficient,
      then we expand the buffer size by 10 % per cycle*/
      expand_buffer_sts = mfx_Bitstream.ExpandBuffer();
#if 0
      if (expand_buffer_sts != MFX_ERR_NONE) {
        blog(LOG_WARNING, "Buffer expansion failed: %d", expand_buffer_sts);
      } else {
        blog(LOG_WARNING, "New BufferSize: %d", mfx_Bitstream.BufferSize());
      }
#endif
      break;
    case MFX_ERR_GPU_HANG:
      blog(LOG_WARNING, "GPU HANG");
      break;
    case MFX_ERR_MORE_SURFACE:
      blog(LOG_WARNING, "NEED MORE SURFACE");
      break;
    /*The buffer is being filled.The error is ignored.*/
    case MFX_ERR_MORE_DATA:
      break;
    default:
#if 0
      blog(LOG_WARNING, "Encode error: %d", sts);
#endif
      break;
    }
  } while (sts == MFX_WRN_IN_EXECUTION || sts == MFX_WRN_DEVICE_BUSY ||
           sts == MFX_ERR_MORE_BITSTREAM || sts == MFX_ERR_NOT_ENOUGH_BUFFER);
  /*Reset Sync Point to use in the next frame*/
  mfx_SyncPoint = nullptr;

  release_sts = mfx_EncodeSurface->FrameInterface->Release(mfx_EncodeSurface);
  if (release_sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Surface release error: %d", release_sts);
  }

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Drain() {
  mfxStatus sts = MFX_ERR_NONE;
  mfxStatus sync_sts = MFX_ERR_NONE;

  if (mfx_Bitstream.Data() != nullptr && mfx_SyncPoint != nullptr) {
    /*Drain bitstream*/
    do {
      sts = mfx_VideoENC->EncodeFrameAsync(nullptr, nullptr,
                                           mfx_Bitstream.Get(), &mfx_SyncPoint);
    } while (sts != MFX_ERR_MORE_DATA);
  }

  if (mfx_SyncPoint != nullptr) {
    do {
      sync_sts = MFXVideoCORE_SyncOperation(mfx_Session, mfx_SyncPoint,
                                            static_cast<mfxU32>(INFINITE));
    } while (sync_sts == MFX_WRN_IN_EXECUTION);
    if (sync_sts < MFX_ERR_NONE) {
      blog(LOG_WARNING, "Drain.Sync error: %d", sts);
      return sync_sts;
    }
  }

  mfx_SyncPoint = nullptr;

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::ClearData() {
  mfxStatus sts = MFX_ERR_NONE;

  if (mfx_VideoENC != nullptr) {
    sts = Drain();

    mfx_EncParams.~ExtBufManager();
    sts = mfx_VideoENC->Close();
    mfx_VideoENC->~MFXVideoENCODE();
    delete mfx_VideoENC;
    mfx_VideoENC = nullptr;
  }

  if (!!mfx_Bitstream.PTR() && mfx_Bitstream.PTR() != nullptr) {
    mfx_Bitstream.Release();
  }

  if (sts >= MFX_ERR_NONE) {
    mfx_OpenEncodersNum--;
  }

  if (mfx_OpenEncodersNum <= 0) {
    mfx_DX_Handle = nullptr;
  }

  if (mfx_Session) {
    MFXClose(mfx_Session);
    MFXDispReleaseImplDescription(mfx_Loader, nullptr);
    MFXUnload(mfx_Loader);
    mfx_Session = nullptr;
    mfx_Loader = nullptr;
  }

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::Reset(struct qsv_param_t *pParams,
                                          enum qsv_codec codec) {
  mfxStatus sts = ClearData();
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Reset.ClearData error: %d", sts);
    return sts;
  }

  sts = Open(pParams, codec);
  if (sts < MFX_ERR_NONE) {
    blog(LOG_WARNING, "Reset.Open error: %d", sts);
    return sts;
  }

  return sts;
}

mfxStatus QSV_VPL_Encoder_Internal::GetCurrentFourCC(mfxU32 &fourCC) {
  if (mfx_VideoENC != nullptr) {
    fourCC = mfx_EncParams.mfx.FrameInfo.FourCC;
    return MFX_ERR_NONE;
  } else {
    return MFX_ERR_NOT_INITIALIZED;
  }
}