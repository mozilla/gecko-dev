// src/glue/messages.ts
var GLUE_VERSION = 1;
var GLUE_MESSAGE_PROTOTYPES = {
  "erro_evt": {
    "name": "erro_evt",
    "structName": "glue_msg_error",
    "className": "GlueMsgError",
    "fields": [
      {
        "type": "str",
        "name": "message",
        "isNullable": false
      }
    ]
  },
  "load_req": {
    "name": "load_req",
    "structName": "glue_msg_load_req",
    "className": "GlueMsgLoadReq",
    "fields": [
      {
        "type": "arr_str",
        "name": "model_paths",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "n_ctx_auto",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "use_mmap",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "use_mlock",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_gpu_layers",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "seed",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_ctx",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_threads",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "embeddings",
        "isNullable": true
      },
      {
        "type": "bool",
        "name": "offload_kqv",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "n_batch",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "n_seq_max",
        "isNullable": true
      },
      {
        "type": "str",
        "name": "pooling_type",
        "isNullable": true
      },
      {
        "type": "str",
        "name": "rope_scaling_type",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "rope_freq_base",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "rope_freq_scale",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "yarn_ext_factor",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "yarn_attn_factor",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "yarn_beta_fast",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "yarn_beta_slow",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "yarn_orig_ctx",
        "isNullable": true
      },
      {
        "type": "str",
        "name": "cache_type_k",
        "isNullable": true
      },
      {
        "type": "str",
        "name": "cache_type_v",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "n_ubatch",
        "isNullable": true
      },
      {
        "type": "bool",
        "name": "flash_attn",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "n_threads_decoding",
        "isNullable": true
      }
    ]
  },
  "load_res": {
    "name": "load_res",
    "structName": "glue_msg_load_res",
    "className": "GlueMsgLoadRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_ctx",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_batch",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_ubatch",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_vocab",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_ctx_train",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_embd",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_layer",
        "isNullable": false
      },
      {
        "type": "arr_str",
        "name": "metadata_key",
        "isNullable": false
      },
      {
        "type": "arr_str",
        "name": "metadata_val",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "token_bos",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "token_eos",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "token_eot",
        "isNullable": false
      },
      {
        "type": "arr_int",
        "name": "list_tokens_eog",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "add_bos_token",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "add_eos_token",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "has_encoder",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "token_decoder_start",
        "isNullable": false
      }
    ]
  },
  "opti_req": {
    "name": "opti_req",
    "structName": "glue_msg_set_options_req",
    "className": "GlueMsgSetOptionsReq",
    "fields": [
      {
        "type": "bool",
        "name": "embeddings",
        "isNullable": false
      }
    ]
  },
  "opti_res": {
    "name": "opti_res",
    "structName": "glue_msg_set_options_res",
    "className": "GlueMsgSetOptionsRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      }
    ]
  },
  "sint_req": {
    "name": "sint_req",
    "structName": "glue_msg_sampling_init_req",
    "className": "GlueMsgSamplingInitReq",
    "fields": [
      {
        "type": "int",
        "name": "mirostat",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "mirostat_tau",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "mirostat_eta",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "temp",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "top_p",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "top_k",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "penalty_last_n",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "penalty_repeat",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "penalty_freq",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "penalty_present",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "dynatemp_range",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "dynatemp_exponent",
        "isNullable": true
      },
      {
        "type": "arr_str",
        "name": "samplers_sequence",
        "isNullable": true
      },
      {
        "type": "str",
        "name": "grammar",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "n_prev",
        "isNullable": true
      },
      {
        "type": "int",
        "name": "n_probs",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "min_p",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "typical_p",
        "isNullable": true
      },
      {
        "type": "float",
        "name": "typ_p",
        "isNullable": true
      },
      {
        "type": "arr_int",
        "name": "logit_bias_toks",
        "isNullable": true
      },
      {
        "type": "arr_float",
        "name": "logit_bias_vals",
        "isNullable": true
      },
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": true
      }
    ]
  },
  "sint_res": {
    "name": "sint_res",
    "structName": "glue_msg_sampling_init_res",
    "className": "GlueMsgSamplingInitRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      }
    ]
  },
  "gvoc_req": {
    "name": "gvoc_req",
    "structName": "glue_msg_get_vocab_req",
    "className": "GlueMsgGetVocabReq",
    "fields": []
  },
  "gvoc_res": {
    "name": "gvoc_res",
    "structName": "glue_msg_get_vocab_res",
    "className": "GlueMsgGetVocabRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "arr_raw",
        "name": "vocab",
        "isNullable": false
      }
    ]
  },
  "lkup_req": {
    "name": "lkup_req",
    "structName": "glue_msg_lookup_token_req",
    "className": "GlueMsgLookupTokenReq",
    "fields": [
      {
        "type": "str",
        "name": "piece",
        "isNullable": false
      }
    ]
  },
  "lkup_res": {
    "name": "lkup_res",
    "structName": "glue_msg_lookup_token_res",
    "className": "GlueMsgLookupTokenRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "token",
        "isNullable": false
      }
    ]
  },
  "tokn_req": {
    "name": "tokn_req",
    "structName": "glue_msg_tokenize_req",
    "className": "GlueMsgTokenizeReq",
    "fields": [
      {
        "type": "str",
        "name": "text",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "special",
        "isNullable": false
      }
    ]
  },
  "tokn_res": {
    "name": "tokn_res",
    "structName": "glue_msg_tokenize_res",
    "className": "GlueMsgTokenizeRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "dtkn_req": {
    "name": "dtkn_req",
    "structName": "glue_msg_detokenize_req",
    "className": "GlueMsgDetokenizeReq",
    "fields": [
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "dtkn_res": {
    "name": "dtkn_res",
    "structName": "glue_msg_detokenize_res",
    "className": "GlueMsgDetokenizeRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "raw",
        "name": "buffer",
        "isNullable": false
      }
    ]
  },
  "deco_req": {
    "name": "deco_req",
    "structName": "glue_msg_decode_req",
    "className": "GlueMsgDecodeReq",
    "fields": [
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "skip_logits",
        "isNullable": false
      }
    ]
  },
  "deco_res": {
    "name": "deco_res",
    "structName": "glue_msg_decode_res",
    "className": "GlueMsgDecodeRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "str",
        "name": "message",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_past",
        "isNullable": false
      }
    ]
  },
  "enco_req": {
    "name": "enco_req",
    "structName": "glue_msg_encode_req",
    "className": "GlueMsgEncodeReq",
    "fields": [
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "enco_res": {
    "name": "enco_res",
    "structName": "glue_msg_encode_res",
    "className": "GlueMsgEncodeRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "str",
        "name": "message",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_past",
        "isNullable": false
      }
    ]
  },
  "ssam_req": {
    "name": "ssam_req",
    "structName": "glue_msg_sampling_sample_req",
    "className": "GlueMsgSamplingSampleReq",
    "fields": []
  },
  "ssam_res": {
    "name": "ssam_res",
    "structName": "glue_msg_sampling_sample_res",
    "className": "GlueMsgSamplingSampleRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "raw",
        "name": "piece",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "token",
        "isNullable": false
      }
    ]
  },
  "sacc_req": {
    "name": "sacc_req",
    "structName": "glue_msg_sampling_accept_req",
    "className": "GlueMsgSamplingAcceptReq",
    "fields": [
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "sacc_res": {
    "name": "sacc_res",
    "structName": "glue_msg_sampling_accept_res",
    "className": "GlueMsgSamplingAcceptRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      }
    ]
  },
  "glog_req": {
    "name": "glog_req",
    "structName": "glue_msg_get_logits_req",
    "className": "GlueMsgGetLogitsReq",
    "fields": [
      {
        "type": "int",
        "name": "top_k",
        "isNullable": false
      }
    ]
  },
  "glog_res": {
    "name": "glog_res",
    "structName": "glue_msg_get_logits_res",
    "className": "GlueMsgGetLogitsRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      },
      {
        "type": "arr_float",
        "name": "probs",
        "isNullable": false
      }
    ]
  },
  "gemb_req": {
    "name": "gemb_req",
    "structName": "glue_msg_get_embeddings_req",
    "className": "GlueMsgGetEmbeddingsReq",
    "fields": [
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "gemb_res": {
    "name": "gemb_res",
    "structName": "glue_msg_get_embeddings_res",
    "className": "GlueMsgGetEmbeddingsRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "str",
        "name": "message",
        "isNullable": false
      },
      {
        "type": "arr_float",
        "name": "embeddings",
        "isNullable": false
      }
    ]
  },
  "kvcr_req": {
    "name": "kvcr_req",
    "structName": "glue_msg_get_kv_remove_req",
    "className": "GlueMsgGetKvRemoveReq",
    "fields": [
      {
        "type": "int",
        "name": "n_keep",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_discard",
        "isNullable": false
      }
    ]
  },
  "kvcr_res": {
    "name": "kvcr_res",
    "structName": "glue_msg_get_kv_remove_res",
    "className": "GlueMsgGetKvRemoveRes",
    "fields": [
      {
        "type": "int",
        "name": "n_past",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      }
    ]
  },
  "kvcc_req": {
    "name": "kvcc_req",
    "structName": "glue_msg_get_kv_clear_req",
    "className": "GlueMsgGetKvClearReq",
    "fields": []
  },
  "kvcc_res": {
    "name": "kvcc_res",
    "structName": "glue_msg_get_kv_clear_res",
    "className": "GlueMsgGetKvClearRes",
    "fields": [
      {
        "type": "int",
        "name": "n_past",
        "isNullable": false
      },
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      }
    ]
  },
  "sesa_req": {
    "name": "sesa_req",
    "structName": "glue_msg_session_save_req",
    "className": "GlueMsgSessionSaveReq",
    "fields": [
      {
        "type": "str",
        "name": "session_path",
        "isNullable": false
      }
    ]
  },
  "sesa_res": {
    "name": "sesa_res",
    "structName": "glue_msg_session_save_res",
    "className": "GlueMsgSessionSaveRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "sesl_req": {
    "name": "sesl_req",
    "structName": "glue_msg_session_load_req",
    "className": "GlueMsgSessionLoadReq",
    "fields": [
      {
        "type": "str",
        "name": "session_path",
        "isNullable": false
      },
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "sesl_res": {
    "name": "sesl_res",
    "structName": "glue_msg_session_load_res",
    "className": "GlueMsgSessionLoadRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      }
    ]
  },
  "stat_req": {
    "name": "stat_req",
    "structName": "glue_msg_status_req",
    "className": "GlueMsgStatusReq",
    "fields": []
  },
  "stat_res": {
    "name": "stat_res",
    "structName": "glue_msg_status_res",
    "className": "GlueMsgStatusRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "tben_req": {
    "name": "tben_req",
    "structName": "glue_msg_test_benchmark_req",
    "className": "GlueMsgTestBenchmarkReq",
    "fields": [
      {
        "type": "str",
        "name": "type",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_samples",
        "isNullable": false
      }
    ]
  },
  "tben_res": {
    "name": "tben_res",
    "structName": "glue_msg_test_benchmark_res",
    "className": "GlueMsgTestBenchmarkRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "str",
        "name": "message",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "t_ms",
        "isNullable": false
      }
    ]
  },
  "tper_req": {
    "name": "tper_req",
    "structName": "glue_msg_test_perplexity_req",
    "className": "GlueMsgTestPerplexityReq",
    "fields": [
      {
        "type": "arr_int",
        "name": "tokens",
        "isNullable": false
      }
    ]
  },
  "tper_res": {
    "name": "tper_res",
    "structName": "glue_msg_test_perplexity_res",
    "className": "GlueMsgTestPerplexityRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "str",
        "name": "message",
        "isNullable": false
      },
      {
        "type": "float",
        "name": "ppl",
        "isNullable": false
      },
      {
        "type": "float",
        "name": "nll",
        "isNullable": false
      },
      {
        "type": "float",
        "name": "cross_entropy",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "n_tokens",
        "isNullable": false
      },
      {
        "type": "int",
        "name": "t_ms",
        "isNullable": false
      }
    ]
  },
  "cfmt_req": {
    "name": "cfmt_req",
    "structName": "glue_msg_chat_format_req",
    "className": "GlueMsgChatFormatReq",
    "fields": [
      {
        "type": "str",
        "name": "tmpl",
        "isNullable": true
      },
      {
        "type": "bool",
        "name": "add_ass",
        "isNullable": true
      },
      {
        "type": "arr_str",
        "name": "roles",
        "isNullable": false
      },
      {
        "type": "arr_str",
        "name": "contents",
        "isNullable": false
      }
    ]
  },
  "cfmt_res": {
    "name": "cfmt_res",
    "structName": "glue_msg_chat_format_res",
    "className": "GlueMsgChatFormatRes",
    "fields": [
      {
        "type": "bool",
        "name": "success",
        "isNullable": false
      },
      {
        "type": "str",
        "name": "message",
        "isNullable": false
      },
      {
        "type": "str",
        "name": "formatted_chat",
        "isNullable": false
      }
    ]
  }
};

// src/glue/glue.ts
var GLUE_MAGIC = new Uint8Array([71, 76, 85, 69]);
var GLUE_DTYPE_NULL = 0;
var GLUE_DTYPE_BOOL = 1;
var GLUE_DTYPE_INT = 2;
var GLUE_DTYPE_FLOAT = 3;
var GLUE_DTYPE_STRING = 4;
var GLUE_DTYPE_RAW = 5;
var GLUE_DTYPE_ARRAY_BOOL = 6;
var GLUE_DTYPE_ARRAY_INT = 7;
var GLUE_DTYPE_ARRAY_FLOAT = 8;
var GLUE_DTYPE_ARRAY_STRING = 9;
var GLUE_DTYPE_ARRAY_RAW = 10;
var TYPE_MAP = {
  str: GLUE_DTYPE_STRING,
  int: GLUE_DTYPE_INT,
  float: GLUE_DTYPE_FLOAT,
  bool: GLUE_DTYPE_BOOL,
  raw: GLUE_DTYPE_RAW,
  arr_str: GLUE_DTYPE_ARRAY_STRING,
  arr_int: GLUE_DTYPE_ARRAY_INT,
  arr_float: GLUE_DTYPE_ARRAY_FLOAT,
  arr_bool: GLUE_DTYPE_ARRAY_BOOL,
  arr_raw: GLUE_DTYPE_ARRAY_RAW,
  null: GLUE_DTYPE_NULL
};
function glueDeserialize(buf) {
  let offset = 0;
  const view = new DataView(buf.buffer);
  const readUint32 = () => {
    const value = view.getUint32(offset, true);
    offset += 4;
    return value;
  };
  const readInt32 = () => {
    const value = view.getInt32(offset, true);
    offset += 4;
    return value;
  };
  const readFloat = () => {
    const value = view.getFloat32(offset, true);
    offset += 4;
    return value;
  };
  const readBool = () => {
    return readUint32() !== 0;
  };
  const readString = (customLen) => {
    const length = customLen ?? readUint32();
    const value = new TextDecoder().decode(buf.slice(offset, offset + length));
    offset += length;
    return value;
  };
  const readRaw = () => {
    const length = readUint32();
    const value = buf.slice(offset, offset + length);
    offset += length;
    return value;
  };
  const readArray = (readItem) => {
    const length = readUint32();
    const value = new Array(length);
    for (let i = 0; i < length; i++) {
      value[i] = readItem();
    }
    return value;
  };
  const readNull = () => null;
  const readField = (field) => {
    switch (field.type) {
      case "str":
        return readString();
      case "int":
        return readInt32();
      case "float":
        return readFloat();
      case "bool":
        return readBool();
      case "raw":
        return readRaw();
      case "arr_str":
        return readArray(readString);
      case "arr_int":
        return readArray(readInt32);
      case "arr_float":
        return readArray(readFloat);
      case "arr_bool":
        return readArray(readBool);
      case "arr_raw":
        return readArray(readRaw);
      case "null":
        return readNull();
    }
  };
  const magicValid = buf[0] === GLUE_MAGIC[0] && buf[1] === GLUE_MAGIC[1] && buf[2] === GLUE_MAGIC[2] && buf[3] === GLUE_MAGIC[3];
  offset += 4;
  if (!magicValid) {
    throw new Error("Invalid magic number");
  }
  const version = readUint32();
  if (version !== GLUE_VERSION) {
    throw new Error("Invalid version number");
  }
  const name = readString(8);
  const msgProto = GLUE_MESSAGE_PROTOTYPES[name];
  if (!msgProto) {
    throw new Error(`Unknown message name: ${name}`);
  }
  const output = { _name: name };
  for (const field of msgProto.fields) {
    const readType = readUint32();
    if (readType === GLUE_DTYPE_NULL) {
      if (!field.isNullable) {
        throw new Error(
          `${name}: Expect field ${field.name} to be non-nullable`
        );
      }
      output[field.name] = null;
      continue;
    }
    if (readType !== TYPE_MAP[field.type]) {
      throw new Error(
        `${name}: Expect field ${field.name} to have type ${field.type}`
      );
    }
    output[field.name] = readField(field);
  }
  return output;
}
function glueSerialize(msg) {
  const msgProto = GLUE_MESSAGE_PROTOTYPES[msg._name];
  if (!msgProto) {
    throw new Error(`Unknown message name: ${msg._name}`);
  }
  const bufs = [];
  const writeUint32 = (value) => {
    const buf = new ArrayBuffer(4);
    new DataView(buf).setUint32(0, value, true);
    bufs.push(new Uint8Array(buf));
  };
  const writeInt32 = (value) => {
    const buf = new ArrayBuffer(4);
    new DataView(buf).setInt32(0, value, true);
    bufs.push(new Uint8Array(buf));
  };
  const writeFloat = (value) => {
    const buf = new ArrayBuffer(4);
    new DataView(buf).setFloat32(0, value, true);
    bufs.push(new Uint8Array(buf));
  };
  const writeBool = (value) => {
    writeUint32(value ? 1 : 0);
  };
  const writeString = (value) => {
    const utf8 = new TextEncoder().encode(value);
    writeUint32(utf8.byteLength);
    bufs.push(utf8);
  };
  const writeRaw = (value) => {
    writeUint32(value.byteLength);
    bufs.push(value);
  };
  const writeArray = (value, writeItem) => {
    writeUint32(value.length);
    for (const item of value) {
      writeItem(item);
    }
  };
  const writeNull = () => {
  };
  bufs.push(GLUE_MAGIC);
  writeUint32(GLUE_VERSION);
  {
    const utf8 = new TextEncoder().encode(msg._name);
    bufs.push(utf8);
  }
  for (const field of msgProto.fields) {
    const val = msg[field.name];
    if (!field.isNullable && (val === null || val === void 0)) {
      throw new Error(
        `${msg._name}: Expect field ${field.name} to be non-nullable`
      );
    }
    if (val === null || val === void 0) {
      writeUint32(GLUE_DTYPE_NULL);
      continue;
    }
    writeUint32(TYPE_MAP[field.type]);
    switch (field.type) {
      case "str":
        writeString(val);
        break;
      case "int":
        writeInt32(val);
        break;
      case "float":
        writeFloat(val);
        break;
      case "bool":
        writeBool(val);
        break;
      case "raw":
        writeRaw(val);
        break;
      case "arr_str":
        writeArray(val, writeString);
        break;
      case "arr_int":
        writeArray(val, writeInt32);
        break;
      case "arr_float":
        writeArray(val, writeFloat);
        break;
      case "arr_bool":
        writeArray(val, writeBool);
        break;
      case "arr_raw":
        writeArray(val, writeRaw);
        break;
      case "null":
        writeNull();
        break;
    }
  }
  const totalLength = bufs.reduce((acc, buf) => acc + buf.byteLength, 0);
  const output = new Uint8Array(totalLength);
  let offset = 0;
  for (const buf of bufs) {
    output.set(buf, offset);
    offset += buf.byteLength;
  }
  return output;
}

// src/utils.ts
var joinBuffers = (buffers) => {
  const totalSize = buffers.reduce((acc, buf) => acc + buf.length, 0);
  const output = new Uint8Array(totalSize);
  output.set(buffers[0], 0);
  for (let i = 1; i < buffers.length; i++) {
    output.set(buffers[i], buffers[i - 1].length);
  }
  return output;
};
var textDecoder = new TextDecoder();
var bufToText = (buffer) => {
  return textDecoder.decode(buffer);
};
var URL_PARTS_REGEX = /-(\d{5})-of-(\d{5})\.gguf$/;
var parseShardNumber = (fnameOrUrl) => {
  const matches = fnameOrUrl.match(URL_PARTS_REGEX);
  if (!matches) {
    return {
      baseURL: fnameOrUrl,
      current: 1,
      total: 1
    };
  } else {
    return {
      baseURL: fnameOrUrl.replace(URL_PARTS_REGEX, ""),
      current: parseInt(matches[1]),
      total: parseInt(matches[2])
    };
  }
};
var sortFileByShard = (blobs) => {
  const isFiles = blobs.every((b) => !!b.name);
  if (isFiles && blobs.length > 1) {
    const files = blobs;
    files.sort((a, b) => {
      const infoA = parseShardNumber(a.name);
      const infoB = parseShardNumber(b.name);
      return infoA.current - infoB.current;
    });
  }
};
var absoluteUrl = (relativePath) => relativePath;
var sumArr = (arr) => arr.reduce((prev, curr) => prev + curr, 0);
var isString = (value) => !!value?.startsWith;
var isSupportMultiThread = () => (async (e) => {
  try {
    return "undefined" != typeof MessageChannel && new MessageChannel().port1.postMessage(new SharedArrayBuffer(1)), WebAssembly.validate(e);
  } catch (e2) {
    return false;
  }
})(
  new Uint8Array([
    0,
    97,
    115,
    109,
    1,
    0,
    0,
    0,
    1,
    4,
    1,
    96,
    0,
    0,
    3,
    2,
    1,
    0,
    5,
    4,
    1,
    3,
    1,
    1,
    10,
    11,
    1,
    9,
    0,
    65,
    0,
    254,
    16,
    2,
    0,
    26,
    11
  ])
);
var isSupportExceptions = async () => WebAssembly.validate(
  new Uint8Array([
    0,
    97,
    115,
    109,
    1,
    0,
    0,
    0,
    1,
    4,
    1,
    96,
    0,
    0,
    3,
    2,
    1,
    0,
    10,
    8,
    1,
    6,
    0,
    6,
    64,
    25,
    11,
    11
  ])
);
var isSupportSIMD = async () => WebAssembly.validate(
  new Uint8Array([
    0,
    97,
    115,
    109,
    1,
    0,
    0,
    0,
    1,
    5,
    1,
    96,
    0,
    1,
    123,
    3,
    2,
    1,
    0,
    10,
    10,
    1,
    8,
    0,
    65,
    0,
    253,
    15,
    253,
    98,
    11
  ])
);
var checkEnvironmentCompatible = async () => {
  if (!await isSupportExceptions()) {
    throw new Error("WebAssembly runtime does not support exception handling");
  }
  if (!await isSupportSIMD()) {
    throw new Error("WebAssembly runtime does not support SIMD");
  }
};
var isSafariMobile = () => {
  return !!navigator.userAgent.match(/Version\/([0-9\._]+).*Mobile.*Safari.*/);
};
var createWorker = (workerCode) => {
  const workerURL = URL.createObjectURL(
    isString(workerCode) ? new Blob([workerCode], { type: "text/javascript" }) : workerCode
  );
  return new Worker(workerURL, { type: "module" });
};
var cbToAsyncIter = (fn) => (...args) => {
  let values = [];
  let resolve;
  values.push(
    new Promise((r) => {
      resolve = r;
    })
  );
  fn(...args, (val, done) => {
    resolve([val, done]);
    values.push(
      new Promise((r) => {
        resolve = r;
      })
    );
  });
  return async function* () {
    let val;
    for (let i = 0, done = false; !done; i++) {
      [val, done] = await values[i];
      delete values[i];
      if (val !== void 0) yield val;
    }
  }();
};

// src/workers-code/generated.ts
var LLAMA_CPP_WORKER_CODE = "// Start the main llama.cpp\nlet wllamaMalloc;\nlet wllamaStart;\nlet wllamaAction;\nlet wllamaExit;\nlet wllamaDebug;\n\nlet Module = null;\n\n//////////////////////////////////////////////////////////////\n// UTILS\n//////////////////////////////////////////////////////////////\n\n// send message back to main thread\nconst msg = (data, transfer) => postMessage(data, transfer);\n\n// Convert CPP log into JS log\nconst cppLogToJSLog = (line) => {\n  const matched = line.match(/@@(DEBUG|INFO|WARN|ERROR)@@(.*)/);\n  return !!matched\n    ? {\n        level: (matched[1] === 'INFO' ? 'debug' : matched[1]).toLowerCase(),\n        text: matched[2],\n      }\n    : { level: 'log', text: line };\n};\n\n// Get module config that forwards stdout/err to main thread\nconst getWModuleConfig = (_argMainScriptBlob) => {\n  var pathConfig = RUN_OPTIONS.pathConfig;\n  var pthreadPoolSize = RUN_OPTIONS.nbThread;\n  var argMainScriptBlob = _argMainScriptBlob;\n\n  if (!pathConfig['wllama.wasm']) {\n    throw new Error('\"wllama.wasm\" is missing in pathConfig');\n  }\n  return {\n    noInitialRun: true,\n    print: function (text) {\n      if (arguments.length > 1)\n        text = Array.prototype.slice.call(arguments).join(' ');\n      msg({ verb: 'console.log', args: [text] });\n    },\n    printErr: function (text) {\n      if (arguments.length > 1)\n        text = Array.prototype.slice.call(arguments).join(' ');\n      const logLine = cppLogToJSLog(text);\n      msg({ verb: 'console.' + logLine.level, args: [logLine.text] });\n    },\n    locateFile: function (filename, basePath) {\n      const p = pathConfig[filename];\n      const truncate = (str) =>\n        str.length > 128 ? `${str.substr(0, 128)}...` : str;\n      if (filename.match(/wllama\\.worker\\.js/)) {\n        msg({\n          verb: 'console.error',\n          args: [\n            '\"wllama.worker.js\" is removed from v2.2.1. Hint: make sure to clear browser\\'s cache.',\n          ],\n        });\n      } else {\n        msg({\n          verb: 'console.debug',\n          args: [`Loading \"${filename}\" from \"${truncate(p)}\"`],\n        });\n        return p;\n      }\n    },\n    mainScriptUrlOrBlob: argMainScriptBlob,\n    pthreadPoolSize,\n    wasmMemory: pthreadPoolSize > 1 ? getWasmMemory() : null,\n    onAbort: function (text) {\n      msg({ verb: 'signal.abort', args: [text] });\n    },\n  };\n};\n\n// Get the memory to be used by wasm. (Only used in multi-thread mode)\n// Because we have a weird OOM issue on iOS, we need to try some values\n// See: https://github.com/emscripten-core/emscripten/issues/19144\n//      https://github.com/godotengine/godot/issues/70621\nconst getWasmMemory = () => {\n  let minBytes = 128 * 1024 * 1024;\n  let maxBytes = 4096 * 1024 * 1024;\n  let stepBytes = 128 * 1024 * 1024;\n  while (maxBytes > minBytes) {\n    try {\n      const wasmMemory = new WebAssembly.Memory({\n        initial: minBytes / 65536,\n        maximum: maxBytes / 65536,\n        shared: true,\n      });\n      return wasmMemory;\n    } catch (e) {\n      maxBytes -= stepBytes;\n      continue; // retry\n    }\n  }\n  throw new Error('Cannot allocate WebAssembly.Memory');\n};\n\n//////////////////////////////////////////////////////////////\n// MEMFS PATCH\n//////////////////////////////////////////////////////////////\n\n/**\n * By default, emscripten uses memfs. The way it works is by\n * allocating new Uint8Array in javascript heap. This is not good\n * because it requires files to be copied to wasm heap each time\n * a file is read.\n *\n * HeapFS is an alternative, which resolves this problem by\n * allocating space for file directly inside wasm heap. This\n * allows us to mmap without doing any copy.\n *\n * For llama.cpp, this is great because we use MAP_SHARED\n *\n * Ref: https://github.com/ngxson/wllama/pull/39\n * Ref: https://github.com/emscripten-core/emscripten/blob/main/src/library_memfs.js\n *\n * Note 29/05/2024 @ngxson\n * Due to ftell() being limited to MAX_LONG, we cannot load files bigger than 2^31 bytes (or 2GB)\n * Ref: https://github.com/emscripten-core/emscripten/blob/main/system/lib/libc/musl/src/stdio/ftell.c\n */\n\nconst fsNameToFile = {}; // map Name => File\nconst fsIdToFile = {}; // map ID => File\nlet currFileId = 0;\n\n// Patch and redirect memfs calls to wllama\nconst patchMEMFS = () => {\n  const m = Module;\n  // save functions\n  m.MEMFS.stream_ops._read = m.MEMFS.stream_ops.read;\n  m.MEMFS.stream_ops._write = m.MEMFS.stream_ops.write;\n  m.MEMFS.stream_ops._llseek = m.MEMFS.stream_ops.llseek;\n  m.MEMFS.stream_ops._allocate = m.MEMFS.stream_ops.allocate;\n  m.MEMFS.stream_ops._mmap = m.MEMFS.stream_ops.mmap;\n  m.MEMFS.stream_ops._msync = m.MEMFS.stream_ops.msync;\n\n  const patchStream = (stream) => {\n    const name = stream.node.name;\n    if (fsNameToFile[name]) {\n      const f = fsNameToFile[name];\n      stream.node.contents = m.HEAPU8.subarray(f.ptr, f.ptr + f.size);\n      stream.node.usedBytes = f.size;\n    }\n  };\n\n  // replace \"read\" functions\n  m.MEMFS.stream_ops.read = function (\n    stream,\n    buffer,\n    offset,\n    length,\n    position\n  ) {\n    patchStream(stream);\n    return m.MEMFS.stream_ops._read(stream, buffer, offset, length, position);\n  };\n  m.MEMFS.ops_table.file.stream.read = m.MEMFS.stream_ops.read;\n\n  // replace \"llseek\" functions\n  m.MEMFS.stream_ops.llseek = function (stream, offset, whence) {\n    patchStream(stream);\n    return m.MEMFS.stream_ops._llseek(stream, offset, whence);\n  };\n  m.MEMFS.ops_table.file.stream.llseek = m.MEMFS.stream_ops.llseek;\n\n  // replace \"mmap\" functions\n  m.MEMFS.stream_ops.mmap = function (stream, length, position, prot, flags) {\n    patchStream(stream);\n    const name = stream.node.name;\n    if (fsNameToFile[name]) {\n      const f = fsNameToFile[name];\n      return {\n        ptr: f.ptr + position,\n        allocated: false,\n      };\n    } else {\n      return m.MEMFS.stream_ops._mmap(stream, length, position, prot, flags);\n    }\n  };\n  m.MEMFS.ops_table.file.stream.mmap = m.MEMFS.stream_ops.mmap;\n\n  // mount FS\n  m.FS.mkdir('/models');\n  m.FS.mount(m.MEMFS, { root: '.' }, '/models');\n};\n\n// Allocate a new file in wllama heapfs, returns file ID\nconst heapfsAlloc = (name, size) => {\n  if (size < 1) {\n    throw new Error('File size must be bigger than 0');\n  }\n  const m = Module;\n  const ptr = m.mmapAlloc(size);\n  const file = {\n    ptr: ptr,\n    size: size,\n    id: currFileId++,\n  };\n  fsIdToFile[file.id] = file;\n  fsNameToFile[name] = file;\n  return file.id;\n};\n\n// Add new file to wllama heapfs, return number of written bytes\nconst heapfsWrite = (id, buffer, offset) => {\n  const m = Module;\n  if (fsIdToFile[id]) {\n    const { ptr, size } = fsIdToFile[id];\n    const afterWriteByte = offset + buffer.byteLength;\n    if (afterWriteByte > size) {\n      throw new Error(\n        `File ID ${id} write out of bound, afterWriteByte = ${afterWriteByte} while size = ${size}`\n      );\n    }\n    m.HEAPU8.set(buffer, ptr + offset);\n    return buffer.byteLength;\n  } else {\n    throw new Error(`File ID ${id} not found in heapfs`);\n  }\n};\n\n//////////////////////////////////////////////////////////////\n// MAIN CODE\n//////////////////////////////////////////////////////////////\n\nconst callWrapper = (name, ret, args) => {\n  const fn = Module.cwrap(name, ret, args);\n  return async (action, req) => {\n    let result;\n    try {\n      if (args.length === 2) {\n        result = await fn(action, req);\n      } else {\n        result = fn();\n      }\n    } catch (ex) {\n      console.error(ex);\n      throw ex;\n    }\n    return result;\n  };\n};\n\nonmessage = async (e) => {\n  if (!e.data) return;\n  const { verb, args, callbackId } = e.data;\n\n  if (!callbackId) {\n    msg({ verb: 'console.error', args: ['callbackId is required', e.data] });\n    return;\n  }\n\n  if (verb === 'module.init') {\n    const argMainScriptBlob = args[0];\n    try {\n      Module = getWModuleConfig(argMainScriptBlob);\n      Module.onRuntimeInitialized = () => {\n        // async call once module is ready\n        // init FS\n        patchMEMFS();\n        // init cwrap\n        const pointer = 'number';\n        // TODO: note sure why emscripten cannot bind if there is only 1 argument\n        wllamaMalloc = callWrapper('wllama_malloc', pointer, [\n          'number',\n          pointer,\n        ]);\n        wllamaStart = callWrapper('wllama_start', 'string', []);\n        wllamaAction = callWrapper('wllama_action', pointer, [\n          'string',\n          pointer,\n        ]);\n        wllamaExit = callWrapper('wllama_exit', 'string', []);\n        wllamaDebug = callWrapper('wllama_debug', 'string', []);\n        msg({ callbackId, result: null });\n      };\n      wModuleInit();\n    } catch (err) {\n      msg({ callbackId, err });\n    }\n    return;\n  }\n\n  if (verb === 'fs.alloc') {\n    const argFilename = args[0];\n    const argSize = args[1];\n    try {\n      // create blank file\n      const emptyBuffer = new ArrayBuffer(0);\n      Module['FS_createDataFile'](\n        '/models',\n        argFilename,\n        emptyBuffer,\n        true,\n        true,\n        true\n      );\n      // alloc data on heap\n      const fileId = heapfsAlloc(argFilename, argSize);\n      msg({ callbackId, result: { fileId } });\n    } catch (err) {\n      msg({ callbackId, err });\n    }\n    return;\n  }\n\n  if (verb === 'fs.write') {\n    const argFileId = args[0];\n    const argBuffer = args[1];\n    const argOffset = args[2];\n    try {\n      const writtenBytes = heapfsWrite(argFileId, argBuffer, argOffset);\n      msg({ callbackId, result: { writtenBytes } });\n    } catch (err) {\n      msg({ callbackId, err });\n    }\n    return;\n  }\n\n  if (verb === 'wllama.start') {\n    try {\n      const result = await wllamaStart();\n      msg({ callbackId, result });\n    } catch (err) {\n      msg({ callbackId, err });\n    }\n    return;\n  }\n\n  if (verb === 'wllama.action') {\n    const argAction = args[0];\n    const argEncodedMsg = args[1];\n    try {\n      const inputPtr = await wllamaMalloc(argEncodedMsg.byteLength, 0);\n      // copy data to wasm heap\n      const inputBuffer = new Uint8Array(\n        Module.HEAPU8.buffer,\n        inputPtr,\n        argEncodedMsg.byteLength\n      );\n      inputBuffer.set(argEncodedMsg, 0);\n      const outputPtr = await wllamaAction(argAction, inputPtr);\n      // length of output buffer is written at the first 4 bytes of input buffer\n      const outputLen = new Uint32Array(Module.HEAPU8.buffer, inputPtr, 1)[0];\n      // copy the output buffer to JS heap\n      const outputBuffer = new Uint8Array(outputLen);\n      const outputSrcView = new Uint8Array(\n        Module.HEAPU8.buffer,\n        outputPtr,\n        outputLen\n      );\n      outputBuffer.set(outputSrcView, 0); // copy it\n      msg({ callbackId, result: outputBuffer }, [outputBuffer.buffer]);\n    } catch (err) {\n      msg({ callbackId, err });\n    }\n    return;\n  }\n\n  if (verb === 'wllama.exit') {\n    try {\n      const result = await wllamaExit();\n      msg({ callbackId, result });\n    } catch (err) {\n      msg({ callbackId, err });\n    }\n    return;\n  }\n\n  if (verb === 'wllama.debug') {\n    try {\n      const result = await wllamaDebug();\n      msg({ callbackId, result });\n    } catch (err) {\n      msg({ callbackId, err });\n    }\n    return;\n  }\n};\n";
var OPFS_UTILS_WORKER_CODE = "let accessHandle;\nlet abortController = new AbortController();\n\nasync function openFile(filename) {\n  const opfsRoot = await navigator.storage.getDirectory();\n  const cacheDir = await opfsRoot.getDirectoryHandle('cache', { create: true });\n  const fileHandler = await cacheDir.getFileHandle(filename, { create: true });\n  accessHandle = await fileHandler.createSyncAccessHandle();\n  accessHandle.truncate(0); // clear file content\n}\n\nasync function writeFile(buf) {\n  accessHandle.write(buf);\n}\n\nasync function closeFile() {\n  accessHandle.flush();\n  accessHandle.close();\n}\n\nasync function writeTextFile(filename, str) {\n  await openFile(filename);\n  await writeFile(new TextEncoder().encode(str));\n  await closeFile();\n}\n\nconst throttled = (func, delay) => {\n  let lastRun = 0;\n  return (...args) => {\n    const now = Date.now();\n    if (now - lastRun > delay) {\n      lastRun = now;\n      func.apply(null, args);\n    }\n  };\n};\n\nconst assertNonNull = (val) => {\n  if (val === null || val === undefined) {\n    throw new Error('OPFS Worker: Assertion failed');\n  }\n};\n\n// respond to main thread\nconst resOK = () => postMessage({ ok: true });\nconst resProgress = (loaded, total) =>\n  postMessage({ progress: { loaded, total } });\nconst resErr = (err) => postMessage({ err });\n\nonmessage = async (e) => {\n  try {\n    if (!e.data) return;\n\n    /**\n     * @param {Object} e.data\n     *\n     * Fine-control FS actions:\n     * - { action: 'open', filename: 'string' }\n     * - { action: 'write', buf: ArrayBuffer }\n     * - { action: 'close' }\n     *\n     * Simple write API:\n     * - { action: 'write-simple', filename: 'string', buf: ArrayBuffer }\n     *\n     * Download API:\n     * - { action: 'download', url: 'string', filename: 'string', options: Object, metadataFileName: 'string' }\n     * - { action: 'download-abort' }\n     */\n    const { action, filename, buf, url, options, metadataFileName } = e.data;\n\n    if (action === 'open') {\n      assertNonNull(filename);\n      await openFile(filename);\n      return resOK();\n    } else if (action === 'write') {\n      assertNonNull(buf);\n      await writeFile(buf);\n      return resOK();\n    } else if (action === 'close') {\n      await closeFile();\n      return resOK();\n    } else if (action === 'write-simple') {\n      assertNonNull(filename);\n      assertNonNull(buf);\n      await openFile(filename);\n      await writeFile(buf);\n      await closeFile();\n      return resOK();\n    } else if (action === 'download') {\n      assertNonNull(url);\n      assertNonNull(filename);\n      assertNonNull(metadataFileName);\n      assertNonNull(options);\n      assertNonNull(options.aborted);\n      abortController = new AbortController();\n      if (options.aborted) abortController.abort();\n      const response = await fetch(url, {\n        ...options,\n        signal: abortController.signal,\n      });\n      const contentLength = response.headers.get('content-length');\n      const etag = (response.headers.get('etag') || '').replace(\n        /[^A-Za-z0-9]/g,\n        ''\n      );\n      const total = parseInt(contentLength, 10);\n      const reader = response.body.getReader();\n      await openFile(filename);\n      let loaded = 0;\n      const throttledProgress = throttled(resProgress, 100);\n      while (true) {\n        const { done, value } = await reader.read();\n        if (done) break;\n        loaded += value.byteLength;\n        await writeFile(value);\n        throttledProgress(loaded, total);\n      }\n      resProgress(total, total); // 100% done\n      await closeFile();\n      // make sure this is in-sync with CacheEntryMetadata\n      await writeTextFile(\n        metadataFileName,\n        JSON.stringify({\n          originalURL: url,\n          originalSize: total,\n          etag,\n        })\n      );\n      return resOK();\n    } else if (action === 'download-abort') {\n      if (abortController) {\n        abortController.abort();\n      }\n      return;\n    }\n\n    throw new Error('OPFS Worker: Invalid action', e.data);\n  } catch (err) {\n    return resErr(err);\n  }\n};\n";
var WLLAMA_MULTI_THREAD_CODE = 'var Module=typeof Module!="undefined"?Module:{};var ENVIRONMENT_IS_WEB=typeof window=="object";var ENVIRONMENT_IS_WORKER=typeof WorkerGlobalScope!="undefined";var ENVIRONMENT_IS_NODE=typeof process=="object"&&typeof process.versions=="object"&&typeof process.versions.node=="string"&&process.type!="renderer";var ENVIRONMENT_IS_PTHREAD=ENVIRONMENT_IS_WORKER&&self.name?.startsWith("em-pthread");if(ENVIRONMENT_IS_NODE){var worker_threads=require("worker_threads");global.Worker=worker_threads.Worker;ENVIRONMENT_IS_WORKER=!worker_threads.isMainThread;ENVIRONMENT_IS_PTHREAD=ENVIRONMENT_IS_WORKER&&worker_threads["workerData"]=="em-pthread"}var moduleOverrides=Object.assign({},Module);var arguments_=[];var thisProgram="./this.program";var quit_=(status,toThrow)=>{throw toThrow};var _scriptName=typeof document!="undefined"?document.currentScript?.src:undefined;if(ENVIRONMENT_IS_NODE){_scriptName=__filename}else if(ENVIRONMENT_IS_WORKER){_scriptName=self.location.href}var scriptDirectory="";function locateFile(path){if(Module["locateFile"]){return Module["locateFile"](path,scriptDirectory)}return scriptDirectory+path}var readAsync,readBinary;if(ENVIRONMENT_IS_NODE){var fs=require("fs");var nodePath=require("path");scriptDirectory=__dirname+"/";readBinary=filename=>{filename=isFileURI(filename)?new URL(filename):filename;var ret=fs.readFileSync(filename);return ret};readAsync=async(filename,binary=true)=>{filename=isFileURI(filename)?new URL(filename):filename;var ret=fs.readFileSync(filename,binary?undefined:"utf8");return ret};if(!Module["thisProgram"]&&process.argv.length>1){thisProgram=process.argv[1].replace(/\\\\/g,"/")}arguments_=process.argv.slice(2);if(typeof module!="undefined"){module["exports"]=Module}quit_=(status,toThrow)=>{process.exitCode=status;throw toThrow}}else if(ENVIRONMENT_IS_WEB||ENVIRONMENT_IS_WORKER){if(ENVIRONMENT_IS_WORKER){scriptDirectory=self.location.href}else if(typeof document!="undefined"&&document.currentScript){scriptDirectory=document.currentScript.src}if(scriptDirectory.startsWith("blob:")){scriptDirectory=""}else{scriptDirectory=scriptDirectory.substr(0,scriptDirectory.replace(/[?#].*/,"").lastIndexOf("/")+1)}if(!ENVIRONMENT_IS_NODE){if(ENVIRONMENT_IS_WORKER){readBinary=url=>{var xhr=new XMLHttpRequest;xhr.open("GET",url,false);xhr.responseType="arraybuffer";xhr.send(null);return new Uint8Array(xhr.response)}}readAsync=async url=>{if(isFileURI(url)){return new Promise((resolve,reject)=>{var xhr=new XMLHttpRequest;xhr.open("GET",url,true);xhr.responseType="arraybuffer";xhr.onload=()=>{if(xhr.status==200||xhr.status==0&&xhr.response){resolve(xhr.response);return}reject(xhr.status)};xhr.onerror=reject;xhr.send(null)})}var response=await fetch(url,{credentials:"same-origin"});if(response.ok){return response.arrayBuffer()}throw new Error(response.status+" : "+response.url)}}}else{}var defaultPrint=console.log.bind(console);var defaultPrintErr=console.error.bind(console);if(ENVIRONMENT_IS_NODE){defaultPrint=(...args)=>fs.writeSync(1,args.join(" ")+"\\n");defaultPrintErr=(...args)=>fs.writeSync(2,args.join(" ")+"\\n")}var out=Module["print"]||defaultPrint;var err=Module["printErr"]||defaultPrintErr;Object.assign(Module,moduleOverrides);moduleOverrides=null;if(Module["arguments"])arguments_=Module["arguments"];if(Module["thisProgram"])thisProgram=Module["thisProgram"];var wasmBinary=Module["wasmBinary"];var wasmMemory;var wasmModule;var ABORT=false;var EXITSTATUS;var HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF32,HEAP64,HEAPU64,HEAPF64;var runtimeInitialized=false;var dataURIPrefix="data:application/octet-stream;base64,";var isDataURI=filename=>filename.startsWith(dataURIPrefix);var isFileURI=filename=>filename.startsWith("file://");function GROWABLE_HEAP_I8(){if(wasmMemory.buffer!=HEAP8.buffer){updateMemoryViews()}return HEAP8}function GROWABLE_HEAP_U8(){if(wasmMemory.buffer!=HEAP8.buffer){updateMemoryViews()}return HEAPU8}function GROWABLE_HEAP_I16(){if(wasmMemory.buffer!=HEAP8.buffer){updateMemoryViews()}return HEAP16}function GROWABLE_HEAP_I32(){if(wasmMemory.buffer!=HEAP8.buffer){updateMemoryViews()}return HEAP32}function GROWABLE_HEAP_U32(){if(wasmMemory.buffer!=HEAP8.buffer){updateMemoryViews()}return HEAPU32}function GROWABLE_HEAP_F32(){if(wasmMemory.buffer!=HEAP8.buffer){updateMemoryViews()}return HEAPF32}function GROWABLE_HEAP_F64(){if(wasmMemory.buffer!=HEAP8.buffer){updateMemoryViews()}return HEAPF64}if(ENVIRONMENT_IS_PTHREAD){var wasmModuleReceived;if(ENVIRONMENT_IS_NODE){var parentPort=worker_threads["parentPort"];parentPort.on("message",msg=>onmessage({data:msg}));Object.assign(globalThis,{self:global,postMessage:msg=>parentPort.postMessage(msg)})}var initializedJS=false;function threadPrintErr(...args){var text=args.join(" ");if(ENVIRONMENT_IS_NODE){fs.writeSync(2,text+"\\n");return}console.error(text)}if(!Module["printErr"])err=threadPrintErr;function threadAlert(...args){var text=args.join(" ");postMessage({cmd:"alert",text,threadId:_pthread_self()})}self.alert=threadAlert;self.onunhandledrejection=e=>{throw e.reason||e};function handleMessage(e){try{var msgData=e["data"];var cmd=msgData.cmd;if(cmd==="load"){let messageQueue=[];self.onmessage=e=>messageQueue.push(e);self.startWorker=instance=>{postMessage({cmd:"loaded"});for(let msg of messageQueue){handleMessage(msg)}self.onmessage=handleMessage};for(const handler of msgData.handlers){if(!Module[handler]||Module[handler].proxy){Module[handler]=(...args)=>{postMessage({cmd:"callHandler",handler,args})};if(handler=="print")out=Module[handler];if(handler=="printErr")err=Module[handler]}}wasmMemory=msgData.wasmMemory;updateMemoryViews();wasmModuleReceived(msgData.wasmModule)}else if(cmd==="run"){establishStackSpace(msgData.pthread_ptr);__emscripten_thread_init(msgData.pthread_ptr,0,0,1,0,0);PThread.receiveObjectTransfer(msgData);PThread.threadInitTLS();__emscripten_thread_mailbox_await(msgData.pthread_ptr);if(!initializedJS){initializedJS=true}try{invokeEntryPoint(msgData.start_routine,msgData.arg)}catch(ex){if(ex!="unwind"){throw ex}}}else if(msgData.target==="setimmediate"){}else if(cmd==="checkMailbox"){if(initializedJS){checkMailbox()}}else if(cmd){err(`worker: received unknown command ${cmd}`);err(msgData)}}catch(ex){__emscripten_thread_crashed();throw ex}}self.onmessage=handleMessage}function updateMemoryViews(){var b=wasmMemory.buffer;Module["HEAP8"]=HEAP8=new Int8Array(b);Module["HEAP16"]=HEAP16=new Int16Array(b);Module["HEAPU8"]=HEAPU8=new Uint8Array(b);Module["HEAPU16"]=HEAPU16=new Uint16Array(b);Module["HEAP32"]=HEAP32=new Int32Array(b);Module["HEAPU32"]=HEAPU32=new Uint32Array(b);Module["HEAPF32"]=HEAPF32=new Float32Array(b);Module["HEAPF64"]=HEAPF64=new Float64Array(b);Module["HEAP64"]=HEAP64=new BigInt64Array(b);Module["HEAPU64"]=HEAPU64=new BigUint64Array(b)}if(!ENVIRONMENT_IS_PTHREAD){if(Module["wasmMemory"]){wasmMemory=Module["wasmMemory"]}else{var INITIAL_MEMORY=Module["INITIAL_MEMORY"]||134217728;wasmMemory=new WebAssembly.Memory({initial:INITIAL_MEMORY/65536,maximum:65536,shared:true})}updateMemoryViews()}var __ATPRERUN__=[];var __ATINIT__=[];var __ATMAIN__=[];var __ATPOSTRUN__=[];function preRun(){if(Module["preRun"]){if(typeof Module["preRun"]=="function")Module["preRun"]=[Module["preRun"]];while(Module["preRun"].length){addOnPreRun(Module["preRun"].shift())}}callRuntimeCallbacks(__ATPRERUN__)}function initRuntime(){runtimeInitialized=true;if(ENVIRONMENT_IS_PTHREAD)return startWorker(Module);if(!Module["noFSInit"]&&!FS.initialized)FS.init();FS.ignorePermissions=false;TTY.init();callRuntimeCallbacks(__ATINIT__)}function preMain(){if(ENVIRONMENT_IS_PTHREAD)return;callRuntimeCallbacks(__ATMAIN__)}function postRun(){if(ENVIRONMENT_IS_PTHREAD)return;if(Module["postRun"]){if(typeof Module["postRun"]=="function")Module["postRun"]=[Module["postRun"]];while(Module["postRun"].length){addOnPostRun(Module["postRun"].shift())}}callRuntimeCallbacks(__ATPOSTRUN__)}function addOnPreRun(cb){__ATPRERUN__.unshift(cb)}function addOnInit(cb){__ATINIT__.unshift(cb)}function addOnPostRun(cb){__ATPOSTRUN__.unshift(cb)}var runDependencies=0;var dependenciesFulfilled=null;function getUniqueRunDependency(id){return id}function addRunDependency(id){runDependencies++;Module["monitorRunDependencies"]?.(runDependencies)}function removeRunDependency(id){runDependencies--;Module["monitorRunDependencies"]?.(runDependencies);if(runDependencies==0){if(dependenciesFulfilled){var callback=dependenciesFulfilled;dependenciesFulfilled=null;callback()}}}function abort(what){Module["onAbort"]?.(what);what="Aborted("+what+")";err(what);ABORT=true;what+=". Build with -sASSERTIONS for more info.";if(runtimeInitialized){___trap()}var e=new WebAssembly.RuntimeError(what);throw e}var wasmBinaryFile;function findWasmBinary(){var f="wllama.wasm";if(!isDataURI(f)){return locateFile(f)}return f}function getBinarySync(file){if(file==wasmBinaryFile&&wasmBinary){return new Uint8Array(wasmBinary)}if(readBinary){return readBinary(file)}throw"both async and sync fetching of the wasm failed"}async function getWasmBinary(binaryFile){if(!wasmBinary){try{var response=await readAsync(binaryFile);return new Uint8Array(response)}catch{}}return getBinarySync(binaryFile)}async function instantiateArrayBuffer(binaryFile,imports){try{var binary=await getWasmBinary(binaryFile);var instance=await WebAssembly.instantiate(binary,imports);return instance}catch(reason){err(`failed to asynchronously prepare wasm: ${reason}`);abort(reason)}}async function instantiateAsync(binary,binaryFile,imports){if(!binary&&typeof WebAssembly.instantiateStreaming=="function"&&!isDataURI(binaryFile)&&!isFileURI(binaryFile)&&!ENVIRONMENT_IS_NODE){try{var response=fetch(binaryFile,{credentials:"same-origin"});var instantiationResult=await WebAssembly.instantiateStreaming(response,imports);return instantiationResult}catch(reason){err(`wasm streaming compile failed: ${reason}`);err("falling back to ArrayBuffer instantiation")}}return instantiateArrayBuffer(binaryFile,imports)}function getWasmImports(){assignWasmImports();return{a:wasmImports}}async function createWasm(){function receiveInstance(instance,module){wasmExports=instance.exports;wasmExports=applySignatureConversions(wasmExports);registerTLSInit(wasmExports["N"]);wasmTable=wasmExports["Q"];addOnInit(wasmExports["G"]);wasmModule=module;removeRunDependency("wasm-instantiate");return wasmExports}addRunDependency("wasm-instantiate");function receiveInstantiationResult(result){return receiveInstance(result["instance"],result["module"])}var info=getWasmImports();if(Module["instantiateWasm"]){try{return Module["instantiateWasm"](info,receiveInstance)}catch(e){err(`Module.instantiateWasm callback failed with error: ${e}`);return false}}if(ENVIRONMENT_IS_PTHREAD){return new Promise(resolve=>{wasmModuleReceived=module=>{var instance=new WebAssembly.Instance(module,getWasmImports());resolve(receiveInstance(instance,module))}})}wasmBinaryFile??=findWasmBinary();var result=await instantiateAsync(wasmBinary,wasmBinaryFile,info);var exports=receiveInstantiationResult(result);return exports}class ExitStatus{name="ExitStatus";constructor(status){this.message=`Program terminated with exit(${status})`;this.status=status}}Module["ExitStatus"]=ExitStatus;var terminateWorker=worker=>{worker.terminate();worker.onmessage=e=>{}};Module["terminateWorker"]=terminateWorker;var cleanupThread=pthread_ptr=>{var worker=PThread.pthreads[pthread_ptr];PThread.returnWorkerToPool(worker)};Module["cleanupThread"]=cleanupThread;var spawnThread=threadParams=>{var worker=PThread.getNewWorker();if(!worker){return 6}PThread.runningWorkers.push(worker);PThread.pthreads[threadParams.pthread_ptr]=worker;worker.pthread_ptr=threadParams.pthread_ptr;var msg={cmd:"run",start_routine:threadParams.startRoutine,arg:threadParams.arg,pthread_ptr:threadParams.pthread_ptr};if(ENVIRONMENT_IS_NODE){worker.unref()}worker.postMessage(msg,threadParams.transferList);return 0};Module["spawnThread"]=spawnThread;var runtimeKeepaliveCounter=0;Module["runtimeKeepaliveCounter"]=runtimeKeepaliveCounter;var keepRuntimeAlive=()=>noExitRuntime||runtimeKeepaliveCounter>0;Module["keepRuntimeAlive"]=keepRuntimeAlive;var stackSave=()=>_emscripten_stack_get_current();Module["stackSave"]=stackSave;var stackRestore=val=>__emscripten_stack_restore(val);Module["stackRestore"]=stackRestore;var stackAlloc=sz=>__emscripten_stack_alloc(sz);Module["stackAlloc"]=stackAlloc;var INT53_MAX=9007199254740992;Module["INT53_MAX"]=INT53_MAX;var INT53_MIN=-9007199254740992;Module["INT53_MIN"]=INT53_MIN;var bigintToI53Checked=num=>num<INT53_MIN||num>INT53_MAX?NaN:Number(num);Module["bigintToI53Checked"]=bigintToI53Checked;var proxyToMainThread=(funcIndex,emAsmAddr,sync,...callArgs)=>{var serializedNumCallArgs=callArgs.length*2;var sp=stackSave();var args=stackAlloc(serializedNumCallArgs*8);var b=args>>>3;for(var i=0;i<callArgs.length;i++){var arg=callArgs[i];if(typeof arg=="bigint"){HEAP64[b+2*i]=1n;HEAP64[b+2*i+1]=arg}else{HEAP64[b+2*i]=0n;GROWABLE_HEAP_F64()[b+2*i+1>>>0]=arg}}var rtn=__emscripten_run_on_main_thread_js(funcIndex,emAsmAddr,serializedNumCallArgs,args,sync);stackRestore(sp);return rtn};Module["proxyToMainThread"]=proxyToMainThread;function _proc_exit(code){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(0,0,1,code);EXITSTATUS=code;if(!keepRuntimeAlive()){PThread.terminateAllThreads();Module["onExit"]?.(code);ABORT=true}quit_(code,new ExitStatus(code))}Module["_proc_exit"]=_proc_exit;var handleException=e=>{if(e instanceof ExitStatus||e=="unwind"){return EXITSTATUS}quit_(1,e)};Module["handleException"]=handleException;function exitOnMainThread(returnCode){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(1,0,0,returnCode);_exit(returnCode)}Module["exitOnMainThread"]=exitOnMainThread;var exitJS=(status,implicit)=>{EXITSTATUS=status;if(ENVIRONMENT_IS_PTHREAD){exitOnMainThread(status);throw"unwind"}_proc_exit(status)};Module["exitJS"]=exitJS;var _exit=exitJS;Module["_exit"]=_exit;var PThread={unusedWorkers:[],runningWorkers:[],tlsInitFunctions:[],pthreads:{},init(){if(!ENVIRONMENT_IS_PTHREAD){PThread.initMainThread()}},initMainThread(){var pthreadPoolSize=Module["pthreadPoolSize"];while(pthreadPoolSize--){PThread.allocateUnusedWorker()}addOnPreRun(()=>{addRunDependency("loading-workers");PThread.loadWasmModuleToAllWorkers(()=>removeRunDependency("loading-workers"))})},terminateAllThreads:()=>{for(var worker of PThread.runningWorkers){terminateWorker(worker)}for(var worker of PThread.unusedWorkers){terminateWorker(worker)}PThread.unusedWorkers=[];PThread.runningWorkers=[];PThread.pthreads={}},returnWorkerToPool:worker=>{var pthread_ptr=worker.pthread_ptr;delete PThread.pthreads[pthread_ptr];PThread.unusedWorkers.push(worker);PThread.runningWorkers.splice(PThread.runningWorkers.indexOf(worker),1);worker.pthread_ptr=0;__emscripten_thread_free_data(pthread_ptr)},receiveObjectTransfer(data){},threadInitTLS(){PThread.tlsInitFunctions.forEach(f=>f())},loadWasmModuleToWorker:worker=>new Promise(onFinishedLoading=>{worker.onmessage=e=>{var d=e["data"];var cmd=d.cmd;if(d.targetThread&&d.targetThread!=_pthread_self()){var targetWorker=PThread.pthreads[d.targetThread];if(targetWorker){targetWorker.postMessage(d,d.transferList)}else{err(`Internal error! Worker sent a message "${cmd}" to target pthread ${d.targetThread}, but that thread no longer exists!`)}return}if(cmd==="checkMailbox"){checkMailbox()}else if(cmd==="spawnThread"){spawnThread(d)}else if(cmd==="cleanupThread"){cleanupThread(d.thread)}else if(cmd==="loaded"){worker.loaded=true;if(ENVIRONMENT_IS_NODE&&!worker.pthread_ptr){worker.unref()}onFinishedLoading(worker)}else if(cmd==="alert"){alert(`Thread ${d.threadId}: ${d.text}`)}else if(d.target==="setimmediate"){worker.postMessage(d)}else if(cmd==="callHandler"){Module[d.handler](...d.args)}else if(cmd){err(`worker sent an unknown command ${cmd}`)}};worker.onerror=e=>{var message="worker sent an error!";err(`${message} ${e.filename}:${e.lineno}: ${e.message}`);throw e};if(ENVIRONMENT_IS_NODE){worker.on("message",data=>worker.onmessage({data}));worker.on("error",e=>worker.onerror(e))}var handlers=[];var knownHandlers=["onExit","onAbort","print","printErr"];for(var handler of knownHandlers){if(Module.propertyIsEnumerable(handler)){handlers.push(handler)}}worker.postMessage({cmd:"load",handlers,wasmMemory,wasmModule})}),loadWasmModuleToAllWorkers(onMaybeReady){if(ENVIRONMENT_IS_PTHREAD){return onMaybeReady()}let pthreadPoolReady=Promise.all(PThread.unusedWorkers.map(PThread.loadWasmModuleToWorker));pthreadPoolReady.then(onMaybeReady)},allocateUnusedWorker(){var worker;var workerOptions={workerData:"em-pthread",name:"em-pthread"};var pthreadMainJs=_scriptName;if(Module["mainScriptUrlOrBlob"]){pthreadMainJs=Module["mainScriptUrlOrBlob"];if(typeof pthreadMainJs!="string"){pthreadMainJs=URL.createObjectURL(pthreadMainJs)}}worker=new Worker(pthreadMainJs,workerOptions);PThread.unusedWorkers.push(worker)},getNewWorker(){if(PThread.unusedWorkers.length==0){PThread.allocateUnusedWorker();PThread.loadWasmModuleToWorker(PThread.unusedWorkers[0])}return PThread.unusedWorkers.pop()}};Module["PThread"]=PThread;var callRuntimeCallbacks=callbacks=>{while(callbacks.length>0){callbacks.shift()(Module)}};Module["callRuntimeCallbacks"]=callRuntimeCallbacks;var establishStackSpace=pthread_ptr=>{updateMemoryViews();var stackHigh=GROWABLE_HEAP_U32()[pthread_ptr+52>>>2>>>0];var stackSize=GROWABLE_HEAP_U32()[pthread_ptr+56>>>2>>>0];var stackLow=stackHigh-stackSize;_emscripten_stack_set_limits(stackHigh,stackLow);stackRestore(stackHigh)};Module["establishStackSpace"]=establishStackSpace;function getValue(ptr,type="i8"){if(type.endsWith("*"))type="*";switch(type){case"i1":return GROWABLE_HEAP_I8()[ptr>>>0];case"i8":return GROWABLE_HEAP_I8()[ptr>>>0];case"i16":return GROWABLE_HEAP_I16()[ptr>>>1>>>0];case"i32":return GROWABLE_HEAP_I32()[ptr>>>2>>>0];case"i64":return HEAP64[ptr>>>3];case"float":return GROWABLE_HEAP_F32()[ptr>>>2>>>0];case"double":return GROWABLE_HEAP_F64()[ptr>>>3>>>0];case"*":return GROWABLE_HEAP_U32()[ptr>>>2>>>0];default:abort(`invalid type for getValue: ${type}`)}}Module["getValue"]=getValue;var wasmTableMirror=[];Module["wasmTableMirror"]=wasmTableMirror;var wasmTable;Module["wasmTable"]=wasmTable;var getWasmTableEntry=funcPtr=>{var func=wasmTableMirror[funcPtr];if(!func){if(funcPtr>=wasmTableMirror.length)wasmTableMirror.length=funcPtr+1;wasmTableMirror[funcPtr]=func=wasmTable.get(funcPtr)}return func};Module["getWasmTableEntry"]=getWasmTableEntry;var invokeEntryPoint=(ptr,arg)=>{runtimeKeepaliveCounter=0;noExitRuntime=0;var result=getWasmTableEntry(ptr)(arg);function finish(result){if(keepRuntimeAlive()){EXITSTATUS=result}else{__emscripten_thread_exit(result)}}finish(result)};Module["invokeEntryPoint"]=invokeEntryPoint;var noExitRuntime=Module["noExitRuntime"]||true;Module["noExitRuntime"]=noExitRuntime;var registerTLSInit=tlsInitFunc=>PThread.tlsInitFunctions.push(tlsInitFunc);Module["registerTLSInit"]=registerTLSInit;function setValue(ptr,value,type="i8"){if(type.endsWith("*"))type="*";switch(type){case"i1":GROWABLE_HEAP_I8()[ptr>>>0]=value;break;case"i8":GROWABLE_HEAP_I8()[ptr>>>0]=value;break;case"i16":GROWABLE_HEAP_I16()[ptr>>>1>>>0]=value;break;case"i32":GROWABLE_HEAP_I32()[ptr>>>2>>>0]=value;break;case"i64":HEAP64[ptr>>>3]=BigInt(value);break;case"float":GROWABLE_HEAP_F32()[ptr>>>2>>>0]=value;break;case"double":GROWABLE_HEAP_F64()[ptr>>>3>>>0]=value;break;case"*":GROWABLE_HEAP_U32()[ptr>>>2>>>0]=value;break;default:abort(`invalid type for setValue: ${type}`)}}Module["setValue"]=setValue;function pthreadCreateProxied(pthread_ptr,attr,startRoutine,arg){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(2,0,1,pthread_ptr,attr,startRoutine,arg);return ___pthread_create_js(pthread_ptr,attr,startRoutine,arg)}Module["pthreadCreateProxied"]=pthreadCreateProxied;var _emscripten_has_threading_support=()=>typeof SharedArrayBuffer!="undefined";Module["_emscripten_has_threading_support"]=_emscripten_has_threading_support;function ___pthread_create_js(pthread_ptr,attr,startRoutine,arg){pthread_ptr>>>=0;attr>>>=0;startRoutine>>>=0;arg>>>=0;if(!_emscripten_has_threading_support()){return 6}var transferList=[];var error=0;if(ENVIRONMENT_IS_PTHREAD&&(transferList.length===0||error)){return pthreadCreateProxied(pthread_ptr,attr,startRoutine,arg)}if(error)return error;var threadParams={startRoutine,pthread_ptr,arg,transferList};if(ENVIRONMENT_IS_PTHREAD){threadParams.cmd="spawnThread";postMessage(threadParams,transferList);return 0}return spawnThread(threadParams)}Module["___pthread_create_js"]=___pthread_create_js;var syscallGetVarargI=()=>{var ret=GROWABLE_HEAP_I32()[+SYSCALLS.varargs>>>2>>>0];SYSCALLS.varargs+=4;return ret};Module["syscallGetVarargI"]=syscallGetVarargI;var syscallGetVarargP=syscallGetVarargI;Module["syscallGetVarargP"]=syscallGetVarargP;var PATH={isAbs:path=>path.charAt(0)==="/",splitPath:filename=>{var splitPathRe=/^(\\/?|)([\\s\\S]*?)((?:\\.{1,2}|[^\\/]+?|)(\\.[^.\\/]*|))(?:[\\/]*)$/;return splitPathRe.exec(filename).slice(1)},normalizeArray:(parts,allowAboveRoot)=>{var up=0;for(var i=parts.length-1;i>=0;i--){var last=parts[i];if(last==="."){parts.splice(i,1)}else if(last===".."){parts.splice(i,1);up++}else if(up){parts.splice(i,1);up--}}if(allowAboveRoot){for(;up;up--){parts.unshift("..")}}return parts},normalize:path=>{var isAbsolute=PATH.isAbs(path),trailingSlash=path.substr(-1)==="/";path=PATH.normalizeArray(path.split("/").filter(p=>!!p),!isAbsolute).join("/");if(!path&&!isAbsolute){path="."}if(path&&trailingSlash){path+="/"}return(isAbsolute?"/":"")+path},dirname:path=>{var result=PATH.splitPath(path),root=result[0],dir=result[1];if(!root&&!dir){return"."}if(dir){dir=dir.substr(0,dir.length-1)}return root+dir},basename:path=>path&&path.match(/([^\\/]+|\\/)\\/*$/)[1],join:(...paths)=>PATH.normalize(paths.join("/")),join2:(l,r)=>PATH.normalize(l+"/"+r)};Module["PATH"]=PATH;var initRandomFill=()=>{if(ENVIRONMENT_IS_NODE){var nodeCrypto=require("crypto");return view=>nodeCrypto.randomFillSync(view)}return view=>view.set(crypto.getRandomValues(new Uint8Array(view.byteLength)))};Module["initRandomFill"]=initRandomFill;var randomFill=view=>{(randomFill=initRandomFill())(view)};Module["randomFill"]=randomFill;var PATH_FS={resolve:(...args)=>{var resolvedPath="",resolvedAbsolute=false;for(var i=args.length-1;i>=-1&&!resolvedAbsolute;i--){var path=i>=0?args[i]:FS.cwd();if(typeof path!="string"){throw new TypeError("Arguments to path.resolve must be strings")}else if(!path){return""}resolvedPath=path+"/"+resolvedPath;resolvedAbsolute=PATH.isAbs(path)}resolvedPath=PATH.normalizeArray(resolvedPath.split("/").filter(p=>!!p),!resolvedAbsolute).join("/");return(resolvedAbsolute?"/":"")+resolvedPath||"."},relative:(from,to)=>{from=PATH_FS.resolve(from).substr(1);to=PATH_FS.resolve(to).substr(1);function trim(arr){var start=0;for(;start<arr.length;start++){if(arr[start]!=="")break}var end=arr.length-1;for(;end>=0;end--){if(arr[end]!=="")break}if(start>end)return[];return arr.slice(start,end-start+1)}var fromParts=trim(from.split("/"));var toParts=trim(to.split("/"));var length=Math.min(fromParts.length,toParts.length);var samePartsLength=length;for(var i=0;i<length;i++){if(fromParts[i]!==toParts[i]){samePartsLength=i;break}}var outputParts=[];for(var i=samePartsLength;i<fromParts.length;i++){outputParts.push("..")}outputParts=outputParts.concat(toParts.slice(samePartsLength));return outputParts.join("/")}};Module["PATH_FS"]=PATH_FS;var UTF8Decoder=typeof TextDecoder!="undefined"?new TextDecoder:undefined;Module["UTF8Decoder"]=UTF8Decoder;var UTF8ArrayToString=(heapOrArray,idx=0,maxBytesToRead=NaN)=>{idx>>>=0;var endIdx=idx+maxBytesToRead;var endPtr=idx;while(heapOrArray[endPtr]&&!(endPtr>=endIdx))++endPtr;if(endPtr-idx>16&&heapOrArray.buffer&&UTF8Decoder){return UTF8Decoder.decode(heapOrArray.buffer instanceof ArrayBuffer?heapOrArray.subarray(idx,endPtr):heapOrArray.slice(idx,endPtr))}var str="";while(idx<endPtr){var u0=heapOrArray[idx++];if(!(u0&128)){str+=String.fromCharCode(u0);continue}var u1=heapOrArray[idx++]&63;if((u0&224)==192){str+=String.fromCharCode((u0&31)<<6|u1);continue}var u2=heapOrArray[idx++]&63;if((u0&240)==224){u0=(u0&15)<<12|u1<<6|u2}else{u0=(u0&7)<<18|u1<<12|u2<<6|heapOrArray[idx++]&63}if(u0<65536){str+=String.fromCharCode(u0)}else{var ch=u0-65536;str+=String.fromCharCode(55296|ch>>10,56320|ch&1023)}}return str};Module["UTF8ArrayToString"]=UTF8ArrayToString;var FS_stdin_getChar_buffer=[];Module["FS_stdin_getChar_buffer"]=FS_stdin_getChar_buffer;var lengthBytesUTF8=str=>{var len=0;for(var i=0;i<str.length;++i){var c=str.charCodeAt(i);if(c<=127){len++}else if(c<=2047){len+=2}else if(c>=55296&&c<=57343){len+=4;++i}else{len+=3}}return len};Module["lengthBytesUTF8"]=lengthBytesUTF8;var stringToUTF8Array=(str,heap,outIdx,maxBytesToWrite)=>{outIdx>>>=0;if(!(maxBytesToWrite>0))return 0;var startIdx=outIdx;var endIdx=outIdx+maxBytesToWrite-1;for(var i=0;i<str.length;++i){var u=str.charCodeAt(i);if(u>=55296&&u<=57343){var u1=str.charCodeAt(++i);u=65536+((u&1023)<<10)|u1&1023}if(u<=127){if(outIdx>=endIdx)break;heap[outIdx++>>>0]=u}else if(u<=2047){if(outIdx+1>=endIdx)break;heap[outIdx++>>>0]=192|u>>6;heap[outIdx++>>>0]=128|u&63}else if(u<=65535){if(outIdx+2>=endIdx)break;heap[outIdx++>>>0]=224|u>>12;heap[outIdx++>>>0]=128|u>>6&63;heap[outIdx++>>>0]=128|u&63}else{if(outIdx+3>=endIdx)break;heap[outIdx++>>>0]=240|u>>18;heap[outIdx++>>>0]=128|u>>12&63;heap[outIdx++>>>0]=128|u>>6&63;heap[outIdx++>>>0]=128|u&63}}heap[outIdx>>>0]=0;return outIdx-startIdx};Module["stringToUTF8Array"]=stringToUTF8Array;function intArrayFromString(stringy,dontAddNull,length){var len=length>0?length:lengthBytesUTF8(stringy)+1;var u8array=new Array(len);var numBytesWritten=stringToUTF8Array(stringy,u8array,0,u8array.length);if(dontAddNull)u8array.length=numBytesWritten;return u8array}Module["intArrayFromString"]=intArrayFromString;var FS_stdin_getChar=()=>{if(!FS_stdin_getChar_buffer.length){var result=null;if(ENVIRONMENT_IS_NODE){var BUFSIZE=256;var buf=Buffer.alloc(BUFSIZE);var bytesRead=0;var fd=process.stdin.fd;try{bytesRead=fs.readSync(fd,buf,0,BUFSIZE)}catch(e){if(e.toString().includes("EOF"))bytesRead=0;else throw e}if(bytesRead>0){result=buf.slice(0,bytesRead).toString("utf-8")}}else if(typeof window!="undefined"&&typeof window.prompt=="function"){result=window.prompt("Input: ");if(result!==null){result+="\\n"}}else{}if(!result){return null}FS_stdin_getChar_buffer=intArrayFromString(result,true)}return FS_stdin_getChar_buffer.shift()};Module["FS_stdin_getChar"]=FS_stdin_getChar;var TTY={ttys:[],init(){},shutdown(){},register(dev,ops){TTY.ttys[dev]={input:[],output:[],ops};FS.registerDevice(dev,TTY.stream_ops)},stream_ops:{open(stream){var tty=TTY.ttys[stream.node.rdev];if(!tty){throw new FS.ErrnoError(43)}stream.tty=tty;stream.seekable=false},close(stream){stream.tty.ops.fsync(stream.tty)},fsync(stream){stream.tty.ops.fsync(stream.tty)},read(stream,buffer,offset,length,pos){if(!stream.tty||!stream.tty.ops.get_char){throw new FS.ErrnoError(60)}var bytesRead=0;for(var i=0;i<length;i++){var result;try{result=stream.tty.ops.get_char(stream.tty)}catch(e){throw new FS.ErrnoError(29)}if(result===undefined&&bytesRead===0){throw new FS.ErrnoError(6)}if(result===null||result===undefined)break;bytesRead++;buffer[offset+i]=result}if(bytesRead){stream.node.atime=Date.now()}return bytesRead},write(stream,buffer,offset,length,pos){if(!stream.tty||!stream.tty.ops.put_char){throw new FS.ErrnoError(60)}try{for(var i=0;i<length;i++){stream.tty.ops.put_char(stream.tty,buffer[offset+i])}}catch(e){throw new FS.ErrnoError(29)}if(length){stream.node.mtime=stream.node.ctime=Date.now()}return i}},default_tty_ops:{get_char(tty){return FS_stdin_getChar()},put_char(tty,val){if(val===null||val===10){out(UTF8ArrayToString(tty.output));tty.output=[]}else{if(val!=0)tty.output.push(val)}},fsync(tty){if(tty.output&&tty.output.length>0){out(UTF8ArrayToString(tty.output));tty.output=[]}},ioctl_tcgets(tty){return{c_iflag:25856,c_oflag:5,c_cflag:191,c_lflag:35387,c_cc:[3,28,127,21,4,0,1,0,17,19,26,0,18,15,23,22,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}},ioctl_tcsets(tty,optional_actions,data){return 0},ioctl_tiocgwinsz(tty){return[24,80]}},default_tty1_ops:{put_char(tty,val){if(val===null||val===10){err(UTF8ArrayToString(tty.output));tty.output=[]}else{if(val!=0)tty.output.push(val)}},fsync(tty){if(tty.output&&tty.output.length>0){err(UTF8ArrayToString(tty.output));tty.output=[]}}}};Module["TTY"]=TTY;var zeroMemory=(address,size)=>{GROWABLE_HEAP_U8().fill(0,address,address+size)};Module["zeroMemory"]=zeroMemory;var alignMemory=(size,alignment)=>Math.ceil(size/alignment)*alignment;Module["alignMemory"]=alignMemory;var mmapAlloc=size=>{size=alignMemory(size,65536);var ptr=_emscripten_builtin_memalign(65536,size);if(ptr)zeroMemory(ptr,size);return ptr};Module["mmapAlloc"]=mmapAlloc;var MEMFS={ops_table:null,mount(mount){return MEMFS.createNode(null,"/",16895,0)},createNode(parent,name,mode,dev){if(FS.isBlkdev(mode)||FS.isFIFO(mode)){throw new FS.ErrnoError(63)}MEMFS.ops_table||={dir:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr,lookup:MEMFS.node_ops.lookup,mknod:MEMFS.node_ops.mknod,rename:MEMFS.node_ops.rename,unlink:MEMFS.node_ops.unlink,rmdir:MEMFS.node_ops.rmdir,readdir:MEMFS.node_ops.readdir,symlink:MEMFS.node_ops.symlink},stream:{llseek:MEMFS.stream_ops.llseek}},file:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr},stream:{llseek:MEMFS.stream_ops.llseek,read:MEMFS.stream_ops.read,write:MEMFS.stream_ops.write,allocate:MEMFS.stream_ops.allocate,mmap:MEMFS.stream_ops.mmap,msync:MEMFS.stream_ops.msync}},link:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr,readlink:MEMFS.node_ops.readlink},stream:{}},chrdev:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr},stream:FS.chrdev_stream_ops}};var node=FS.createNode(parent,name,mode,dev);if(FS.isDir(node.mode)){node.node_ops=MEMFS.ops_table.dir.node;node.stream_ops=MEMFS.ops_table.dir.stream;node.contents={}}else if(FS.isFile(node.mode)){node.node_ops=MEMFS.ops_table.file.node;node.stream_ops=MEMFS.ops_table.file.stream;node.usedBytes=0;node.contents=null}else if(FS.isLink(node.mode)){node.node_ops=MEMFS.ops_table.link.node;node.stream_ops=MEMFS.ops_table.link.stream}else if(FS.isChrdev(node.mode)){node.node_ops=MEMFS.ops_table.chrdev.node;node.stream_ops=MEMFS.ops_table.chrdev.stream}node.atime=node.mtime=node.ctime=Date.now();if(parent){parent.contents[name]=node;parent.atime=parent.mtime=parent.ctime=node.atime}return node},getFileDataAsTypedArray(node){if(!node.contents)return new Uint8Array(0);if(node.contents.subarray)return node.contents.subarray(0,node.usedBytes);return new Uint8Array(node.contents)},expandFileStorage(node,newCapacity){var prevCapacity=node.contents?node.contents.length:0;if(prevCapacity>=newCapacity)return;var CAPACITY_DOUBLING_MAX=1024*1024;newCapacity=Math.max(newCapacity,prevCapacity*(prevCapacity<CAPACITY_DOUBLING_MAX?2:1.125)>>>0);if(prevCapacity!=0)newCapacity=Math.max(newCapacity,256);var oldContents=node.contents;node.contents=new Uint8Array(newCapacity);if(node.usedBytes>0)node.contents.set(oldContents.subarray(0,node.usedBytes),0)},resizeFileStorage(node,newSize){if(node.usedBytes==newSize)return;if(newSize==0){node.contents=null;node.usedBytes=0}else{var oldContents=node.contents;node.contents=new Uint8Array(newSize);if(oldContents){node.contents.set(oldContents.subarray(0,Math.min(newSize,node.usedBytes)))}node.usedBytes=newSize}},node_ops:{getattr(node){var attr={};attr.dev=FS.isChrdev(node.mode)?node.id:1;attr.ino=node.id;attr.mode=node.mode;attr.nlink=1;attr.uid=0;attr.gid=0;attr.rdev=node.rdev;if(FS.isDir(node.mode)){attr.size=4096}else if(FS.isFile(node.mode)){attr.size=node.usedBytes}else if(FS.isLink(node.mode)){attr.size=node.link.length}else{attr.size=0}attr.atime=new Date(node.atime);attr.mtime=new Date(node.mtime);attr.ctime=new Date(node.ctime);attr.blksize=4096;attr.blocks=Math.ceil(attr.size/attr.blksize);return attr},setattr(node,attr){for(const key of["mode","atime","mtime","ctime"]){if(attr[key]!=null){node[key]=attr[key]}}if(attr.size!==undefined){MEMFS.resizeFileStorage(node,attr.size)}},lookup(parent,name){throw MEMFS.doesNotExistError},mknod(parent,name,mode,dev){return MEMFS.createNode(parent,name,mode,dev)},rename(old_node,new_dir,new_name){var new_node;try{new_node=FS.lookupNode(new_dir,new_name)}catch(e){}if(new_node){if(FS.isDir(old_node.mode)){for(var i in new_node.contents){throw new FS.ErrnoError(55)}}FS.hashRemoveNode(new_node)}delete old_node.parent.contents[old_node.name];new_dir.contents[new_name]=old_node;old_node.name=new_name;new_dir.ctime=new_dir.mtime=old_node.parent.ctime=old_node.parent.mtime=Date.now()},unlink(parent,name){delete parent.contents[name];parent.ctime=parent.mtime=Date.now()},rmdir(parent,name){var node=FS.lookupNode(parent,name);for(var i in node.contents){throw new FS.ErrnoError(55)}delete parent.contents[name];parent.ctime=parent.mtime=Date.now()},readdir(node){return[".","..",...Object.keys(node.contents)]},symlink(parent,newname,oldpath){var node=MEMFS.createNode(parent,newname,511|40960,0);node.link=oldpath;return node},readlink(node){if(!FS.isLink(node.mode)){throw new FS.ErrnoError(28)}return node.link}},stream_ops:{read(stream,buffer,offset,length,position){var contents=stream.node.contents;if(position>=stream.node.usedBytes)return 0;var size=Math.min(stream.node.usedBytes-position,length);if(size>8&&contents.subarray){buffer.set(contents.subarray(position,position+size),offset)}else{for(var i=0;i<size;i++)buffer[offset+i]=contents[position+i]}return size},write(stream,buffer,offset,length,position,canOwn){if(buffer.buffer===GROWABLE_HEAP_I8().buffer){canOwn=false}if(!length)return 0;var node=stream.node;node.mtime=node.ctime=Date.now();if(buffer.subarray&&(!node.contents||node.contents.subarray)){if(canOwn){node.contents=buffer.subarray(offset,offset+length);node.usedBytes=length;return length}else if(node.usedBytes===0&&position===0){node.contents=buffer.slice(offset,offset+length);node.usedBytes=length;return length}else if(position+length<=node.usedBytes){node.contents.set(buffer.subarray(offset,offset+length),position);return length}}MEMFS.expandFileStorage(node,position+length);if(node.contents.subarray&&buffer.subarray){node.contents.set(buffer.subarray(offset,offset+length),position)}else{for(var i=0;i<length;i++){node.contents[position+i]=buffer[offset+i]}}node.usedBytes=Math.max(node.usedBytes,position+length);return length},llseek(stream,offset,whence){var position=offset;if(whence===1){position+=stream.position}else if(whence===2){if(FS.isFile(stream.node.mode)){position+=stream.node.usedBytes}}if(position<0){throw new FS.ErrnoError(28)}return position},allocate(stream,offset,length){MEMFS.expandFileStorage(stream.node,offset+length);stream.node.usedBytes=Math.max(stream.node.usedBytes,offset+length)},mmap(stream,length,position,prot,flags){if(!FS.isFile(stream.node.mode)){throw new FS.ErrnoError(43)}var ptr;var allocated;var contents=stream.node.contents;if(!(flags&2)&&contents&&contents.buffer===GROWABLE_HEAP_I8().buffer){allocated=false;ptr=contents.byteOffset}else{allocated=true;ptr=mmapAlloc(length);if(!ptr){throw new FS.ErrnoError(48)}if(contents){if(position>0||position+length<contents.length){if(contents.subarray){contents=contents.subarray(position,position+length)}else{contents=Array.prototype.slice.call(contents,position,position+length)}}GROWABLE_HEAP_I8().set(contents,ptr>>>0)}}return{ptr,allocated}},msync(stream,buffer,offset,length,mmapFlags){MEMFS.stream_ops.write(stream,buffer,0,length,offset,false);return 0}}};Module["MEMFS"]=MEMFS;var asyncLoad=async url=>{var arrayBuffer=await readAsync(url);return new Uint8Array(arrayBuffer)};Module["asyncLoad"]=asyncLoad;var FS_createDataFile=(parent,name,fileData,canRead,canWrite,canOwn)=>{FS.createDataFile(parent,name,fileData,canRead,canWrite,canOwn)};Module["FS_createDataFile"]=FS_createDataFile;var preloadPlugins=Module["preloadPlugins"]||[];Module["preloadPlugins"]=preloadPlugins;var FS_handledByPreloadPlugin=(byteArray,fullname,finish,onerror)=>{if(typeof Browser!="undefined")Browser.init();var handled=false;preloadPlugins.forEach(plugin=>{if(handled)return;if(plugin["canHandle"](fullname)){plugin["handle"](byteArray,fullname,finish,onerror);handled=true}});return handled};Module["FS_handledByPreloadPlugin"]=FS_handledByPreloadPlugin;var FS_createPreloadedFile=(parent,name,url,canRead,canWrite,onload,onerror,dontCreateFile,canOwn,preFinish)=>{var fullname=name?PATH_FS.resolve(PATH.join2(parent,name)):parent;var dep=getUniqueRunDependency(`cp ${fullname}`);function processData(byteArray){function finish(byteArray){preFinish?.();if(!dontCreateFile){FS_createDataFile(parent,name,byteArray,canRead,canWrite,canOwn)}onload?.();removeRunDependency(dep)}if(FS_handledByPreloadPlugin(byteArray,fullname,finish,()=>{onerror?.();removeRunDependency(dep)})){return}finish(byteArray)}addRunDependency(dep);if(typeof url=="string"){asyncLoad(url).then(processData,onerror)}else{processData(url)}};Module["FS_createPreloadedFile"]=FS_createPreloadedFile;var FS_modeStringToFlags=str=>{var flagModes={r:0,"r+":2,w:512|64|1,"w+":512|64|2,a:1024|64|1,"a+":1024|64|2};var flags=flagModes[str];if(typeof flags=="undefined"){throw new Error(`Unknown file open mode: ${str}`)}return flags};Module["FS_modeStringToFlags"]=FS_modeStringToFlags;var FS_getMode=(canRead,canWrite)=>{var mode=0;if(canRead)mode|=292|73;if(canWrite)mode|=146;return mode};Module["FS_getMode"]=FS_getMode;var FS={root:null,mounts:[],devices:{},streams:[],nextInode:1,nameTable:null,currentPath:"/",initialized:false,ignorePermissions:true,ErrnoError:class{name="ErrnoError";constructor(errno){this.errno=errno}},filesystems:null,syncFSRequests:0,readFiles:{},FSStream:class{shared={};get object(){return this.node}set object(val){this.node=val}get isRead(){return(this.flags&2097155)!==1}get isWrite(){return(this.flags&2097155)!==0}get isAppend(){return this.flags&1024}get flags(){return this.shared.flags}set flags(val){this.shared.flags=val}get position(){return this.shared.position}set position(val){this.shared.position=val}},FSNode:class{node_ops={};stream_ops={};readMode=292|73;writeMode=146;mounted=null;constructor(parent,name,mode,rdev){if(!parent){parent=this}this.parent=parent;this.mount=parent.mount;this.id=FS.nextInode++;this.name=name;this.mode=mode;this.rdev=rdev;this.atime=this.mtime=this.ctime=Date.now()}get read(){return(this.mode&this.readMode)===this.readMode}set read(val){val?this.mode|=this.readMode:this.mode&=~this.readMode}get write(){return(this.mode&this.writeMode)===this.writeMode}set write(val){val?this.mode|=this.writeMode:this.mode&=~this.writeMode}get isFolder(){return FS.isDir(this.mode)}get isDevice(){return FS.isChrdev(this.mode)}},lookupPath(path,opts={}){if(!path){throw new FS.ErrnoError(44)}opts.follow_mount??=true;if(!PATH.isAbs(path)){path=FS.cwd()+"/"+path}linkloop:for(var nlinks=0;nlinks<40;nlinks++){var parts=path.split("/").filter(p=>!!p);var current=FS.root;var current_path="/";for(var i=0;i<parts.length;i++){var islast=i===parts.length-1;if(islast&&opts.parent){break}if(parts[i]==="."){continue}if(parts[i]===".."){current_path=PATH.dirname(current_path);current=current.parent;continue}current_path=PATH.join2(current_path,parts[i]);try{current=FS.lookupNode(current,parts[i])}catch(e){if(e?.errno===44&&islast&&opts.noent_okay){return{path:current_path}}throw e}if(FS.isMountpoint(current)&&(!islast||opts.follow_mount)){current=current.mounted.root}if(FS.isLink(current.mode)&&(!islast||opts.follow)){if(!current.node_ops.readlink){throw new FS.ErrnoError(52)}var link=current.node_ops.readlink(current);if(!PATH.isAbs(link)){link=PATH.dirname(current_path)+"/"+link}path=link+"/"+parts.slice(i+1).join("/");continue linkloop}}return{path:current_path,node:current}}throw new FS.ErrnoError(32)},getPath(node){var path;while(true){if(FS.isRoot(node)){var mount=node.mount.mountpoint;if(!path)return mount;return mount[mount.length-1]!=="/"?`${mount}/${path}`:mount+path}path=path?`${node.name}/${path}`:node.name;node=node.parent}},hashName(parentid,name){var hash=0;for(var i=0;i<name.length;i++){hash=(hash<<5)-hash+name.charCodeAt(i)|0}return(parentid+hash>>>0)%FS.nameTable.length},hashAddNode(node){var hash=FS.hashName(node.parent.id,node.name);node.name_next=FS.nameTable[hash];FS.nameTable[hash]=node},hashRemoveNode(node){var hash=FS.hashName(node.parent.id,node.name);if(FS.nameTable[hash]===node){FS.nameTable[hash]=node.name_next}else{var current=FS.nameTable[hash];while(current){if(current.name_next===node){current.name_next=node.name_next;break}current=current.name_next}}},lookupNode(parent,name){var errCode=FS.mayLookup(parent);if(errCode){throw new FS.ErrnoError(errCode)}var hash=FS.hashName(parent.id,name);for(var node=FS.nameTable[hash];node;node=node.name_next){var nodeName=node.name;if(node.parent.id===parent.id&&nodeName===name){return node}}return FS.lookup(parent,name)},createNode(parent,name,mode,rdev){var node=new FS.FSNode(parent,name,mode,rdev);FS.hashAddNode(node);return node},destroyNode(node){FS.hashRemoveNode(node)},isRoot(node){return node===node.parent},isMountpoint(node){return!!node.mounted},isFile(mode){return(mode&61440)===32768},isDir(mode){return(mode&61440)===16384},isLink(mode){return(mode&61440)===40960},isChrdev(mode){return(mode&61440)===8192},isBlkdev(mode){return(mode&61440)===24576},isFIFO(mode){return(mode&61440)===4096},isSocket(mode){return(mode&49152)===49152},flagsToPermissionString(flag){var perms=["r","w","rw"][flag&3];if(flag&512){perms+="w"}return perms},nodePermissions(node,perms){if(FS.ignorePermissions){return 0}if(perms.includes("r")&&!(node.mode&292)){return 2}else if(perms.includes("w")&&!(node.mode&146)){return 2}else if(perms.includes("x")&&!(node.mode&73)){return 2}return 0},mayLookup(dir){if(!FS.isDir(dir.mode))return 54;var errCode=FS.nodePermissions(dir,"x");if(errCode)return errCode;if(!dir.node_ops.lookup)return 2;return 0},mayCreate(dir,name){if(!FS.isDir(dir.mode)){return 54}try{var node=FS.lookupNode(dir,name);return 20}catch(e){}return FS.nodePermissions(dir,"wx")},mayDelete(dir,name,isdir){var node;try{node=FS.lookupNode(dir,name)}catch(e){return e.errno}var errCode=FS.nodePermissions(dir,"wx");if(errCode){return errCode}if(isdir){if(!FS.isDir(node.mode)){return 54}if(FS.isRoot(node)||FS.getPath(node)===FS.cwd()){return 10}}else{if(FS.isDir(node.mode)){return 31}}return 0},mayOpen(node,flags){if(!node){return 44}if(FS.isLink(node.mode)){return 32}else if(FS.isDir(node.mode)){if(FS.flagsToPermissionString(flags)!=="r"||flags&(512|64)){return 31}}return FS.nodePermissions(node,FS.flagsToPermissionString(flags))},checkOpExists(op,err){if(!op){throw new FS.ErrnoError(err)}return op},MAX_OPEN_FDS:4096,nextfd(){for(var fd=0;fd<=FS.MAX_OPEN_FDS;fd++){if(!FS.streams[fd]){return fd}}throw new FS.ErrnoError(33)},getStreamChecked(fd){var stream=FS.getStream(fd);if(!stream){throw new FS.ErrnoError(8)}return stream},getStream:fd=>FS.streams[fd],createStream(stream,fd=-1){stream=Object.assign(new FS.FSStream,stream);if(fd==-1){fd=FS.nextfd()}stream.fd=fd;FS.streams[fd]=stream;return stream},closeStream(fd){FS.streams[fd]=null},dupStream(origStream,fd=-1){var stream=FS.createStream(origStream,fd);stream.stream_ops?.dup?.(stream);return stream},chrdev_stream_ops:{open(stream){var device=FS.getDevice(stream.node.rdev);stream.stream_ops=device.stream_ops;stream.stream_ops.open?.(stream)},llseek(){throw new FS.ErrnoError(70)}},major:dev=>dev>>8,minor:dev=>dev&255,makedev:(ma,mi)=>ma<<8|mi,registerDevice(dev,ops){FS.devices[dev]={stream_ops:ops}},getDevice:dev=>FS.devices[dev],getMounts(mount){var mounts=[];var check=[mount];while(check.length){var m=check.pop();mounts.push(m);check.push(...m.mounts)}return mounts},syncfs(populate,callback){if(typeof populate=="function"){callback=populate;populate=false}FS.syncFSRequests++;if(FS.syncFSRequests>1){err(`warning: ${FS.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`)}var mounts=FS.getMounts(FS.root.mount);var completed=0;function doCallback(errCode){FS.syncFSRequests--;return callback(errCode)}function done(errCode){if(errCode){if(!done.errored){done.errored=true;return doCallback(errCode)}return}if(++completed>=mounts.length){doCallback(null)}}mounts.forEach(mount=>{if(!mount.type.syncfs){return done(null)}mount.type.syncfs(mount,populate,done)})},mount(type,opts,mountpoint){var root=mountpoint==="/";var pseudo=!mountpoint;var node;if(root&&FS.root){throw new FS.ErrnoError(10)}else if(!root&&!pseudo){var lookup=FS.lookupPath(mountpoint,{follow_mount:false});mountpoint=lookup.path;node=lookup.node;if(FS.isMountpoint(node)){throw new FS.ErrnoError(10)}if(!FS.isDir(node.mode)){throw new FS.ErrnoError(54)}}var mount={type,opts,mountpoint,mounts:[]};var mountRoot=type.mount(mount);mountRoot.mount=mount;mount.root=mountRoot;if(root){FS.root=mountRoot}else if(node){node.mounted=mount;if(node.mount){node.mount.mounts.push(mount)}}return mountRoot},unmount(mountpoint){var lookup=FS.lookupPath(mountpoint,{follow_mount:false});if(!FS.isMountpoint(lookup.node)){throw new FS.ErrnoError(28)}var node=lookup.node;var mount=node.mounted;var mounts=FS.getMounts(mount);Object.keys(FS.nameTable).forEach(hash=>{var current=FS.nameTable[hash];while(current){var next=current.name_next;if(mounts.includes(current.mount)){FS.destroyNode(current)}current=next}});node.mounted=null;var idx=node.mount.mounts.indexOf(mount);node.mount.mounts.splice(idx,1)},lookup(parent,name){return parent.node_ops.lookup(parent,name)},mknod(path,mode,dev){var lookup=FS.lookupPath(path,{parent:true});var parent=lookup.node;var name=PATH.basename(path);if(!name){throw new FS.ErrnoError(28)}if(name==="."||name===".."){throw new FS.ErrnoError(20)}var errCode=FS.mayCreate(parent,name);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.mknod){throw new FS.ErrnoError(63)}return parent.node_ops.mknod(parent,name,mode,dev)},statfs(path){var rtn={bsize:4096,frsize:4096,blocks:1e6,bfree:5e5,bavail:5e5,files:FS.nextInode,ffree:FS.nextInode-1,fsid:42,flags:2,namelen:255};var parent=FS.lookupPath(path,{follow:true}).node;if(parent?.node_ops.statfs){Object.assign(rtn,parent.node_ops.statfs(parent.mount.opts.root))}return rtn},create(path,mode=438){mode&=4095;mode|=32768;return FS.mknod(path,mode,0)},mkdir(path,mode=511){mode&=511|512;mode|=16384;return FS.mknod(path,mode,0)},mkdirTree(path,mode){var dirs=path.split("/");var d="";for(var i=0;i<dirs.length;++i){if(!dirs[i])continue;d+="/"+dirs[i];try{FS.mkdir(d,mode)}catch(e){if(e.errno!=20)throw e}}},mkdev(path,mode,dev){if(typeof dev=="undefined"){dev=mode;mode=438}mode|=8192;return FS.mknod(path,mode,dev)},symlink(oldpath,newpath){if(!PATH_FS.resolve(oldpath)){throw new FS.ErrnoError(44)}var lookup=FS.lookupPath(newpath,{parent:true});var parent=lookup.node;if(!parent){throw new FS.ErrnoError(44)}var newname=PATH.basename(newpath);var errCode=FS.mayCreate(parent,newname);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.symlink){throw new FS.ErrnoError(63)}return parent.node_ops.symlink(parent,newname,oldpath)},rename(old_path,new_path){var old_dirname=PATH.dirname(old_path);var new_dirname=PATH.dirname(new_path);var old_name=PATH.basename(old_path);var new_name=PATH.basename(new_path);var lookup,old_dir,new_dir;lookup=FS.lookupPath(old_path,{parent:true});old_dir=lookup.node;lookup=FS.lookupPath(new_path,{parent:true});new_dir=lookup.node;if(!old_dir||!new_dir)throw new FS.ErrnoError(44);if(old_dir.mount!==new_dir.mount){throw new FS.ErrnoError(75)}var old_node=FS.lookupNode(old_dir,old_name);var relative=PATH_FS.relative(old_path,new_dirname);if(relative.charAt(0)!=="."){throw new FS.ErrnoError(28)}relative=PATH_FS.relative(new_path,old_dirname);if(relative.charAt(0)!=="."){throw new FS.ErrnoError(55)}var new_node;try{new_node=FS.lookupNode(new_dir,new_name)}catch(e){}if(old_node===new_node){return}var isdir=FS.isDir(old_node.mode);var errCode=FS.mayDelete(old_dir,old_name,isdir);if(errCode){throw new FS.ErrnoError(errCode)}errCode=new_node?FS.mayDelete(new_dir,new_name,isdir):FS.mayCreate(new_dir,new_name);if(errCode){throw new FS.ErrnoError(errCode)}if(!old_dir.node_ops.rename){throw new FS.ErrnoError(63)}if(FS.isMountpoint(old_node)||new_node&&FS.isMountpoint(new_node)){throw new FS.ErrnoError(10)}if(new_dir!==old_dir){errCode=FS.nodePermissions(old_dir,"w");if(errCode){throw new FS.ErrnoError(errCode)}}FS.hashRemoveNode(old_node);try{old_dir.node_ops.rename(old_node,new_dir,new_name);old_node.parent=new_dir}catch(e){throw e}finally{FS.hashAddNode(old_node)}},rmdir(path){var lookup=FS.lookupPath(path,{parent:true});var parent=lookup.node;var name=PATH.basename(path);var node=FS.lookupNode(parent,name);var errCode=FS.mayDelete(parent,name,true);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.rmdir){throw new FS.ErrnoError(63)}if(FS.isMountpoint(node)){throw new FS.ErrnoError(10)}parent.node_ops.rmdir(parent,name);FS.destroyNode(node)},readdir(path){var lookup=FS.lookupPath(path,{follow:true});var node=lookup.node;var readdir=FS.checkOpExists(node.node_ops.readdir,54);return readdir(node)},unlink(path){var lookup=FS.lookupPath(path,{parent:true});var parent=lookup.node;if(!parent){throw new FS.ErrnoError(44)}var name=PATH.basename(path);var node=FS.lookupNode(parent,name);var errCode=FS.mayDelete(parent,name,false);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.unlink){throw new FS.ErrnoError(63)}if(FS.isMountpoint(node)){throw new FS.ErrnoError(10)}parent.node_ops.unlink(parent,name);FS.destroyNode(node)},readlink(path){var lookup=FS.lookupPath(path);var link=lookup.node;if(!link){throw new FS.ErrnoError(44)}if(!link.node_ops.readlink){throw new FS.ErrnoError(28)}return link.node_ops.readlink(link)},stat(path,dontFollow){var lookup=FS.lookupPath(path,{follow:!dontFollow});var node=lookup.node;var getattr=FS.checkOpExists(node.node_ops.getattr,63);return getattr(node)},lstat(path){return FS.stat(path,true)},chmod(path,mode,dontFollow){var node;if(typeof path=="string"){var lookup=FS.lookupPath(path,{follow:!dontFollow});node=lookup.node}else{node=path}var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{mode:mode&4095|node.mode&~4095,ctime:Date.now(),dontFollow})},lchmod(path,mode){FS.chmod(path,mode,true)},fchmod(fd,mode){var stream=FS.getStreamChecked(fd);FS.chmod(stream.node,mode)},chown(path,uid,gid,dontFollow){var node;if(typeof path=="string"){var lookup=FS.lookupPath(path,{follow:!dontFollow});node=lookup.node}else{node=path}var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{timestamp:Date.now(),dontFollow})},lchown(path,uid,gid){FS.chown(path,uid,gid,true)},fchown(fd,uid,gid){var stream=FS.getStreamChecked(fd);FS.chown(stream.node,uid,gid)},truncate(path,len){if(len<0){throw new FS.ErrnoError(28)}var node;if(typeof path=="string"){var lookup=FS.lookupPath(path,{follow:true});node=lookup.node}else{node=path}if(FS.isDir(node.mode)){throw new FS.ErrnoError(31)}if(!FS.isFile(node.mode)){throw new FS.ErrnoError(28)}var errCode=FS.nodePermissions(node,"w");if(errCode){throw new FS.ErrnoError(errCode)}var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{size:len,timestamp:Date.now()})},ftruncate(fd,len){var stream=FS.getStreamChecked(fd);if((stream.flags&2097155)===0){throw new FS.ErrnoError(28)}FS.truncate(stream.node,len)},utime(path,atime,mtime){var lookup=FS.lookupPath(path,{follow:true});var node=lookup.node;var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{atime,mtime})},open(path,flags,mode=438){if(path===""){throw new FS.ErrnoError(44)}flags=typeof flags=="string"?FS_modeStringToFlags(flags):flags;if(flags&64){mode=mode&4095|32768}else{mode=0}var node;var isDirPath;if(typeof path=="object"){node=path}else{isDirPath=path.endsWith("/");var lookup=FS.lookupPath(path,{follow:!(flags&131072),noent_okay:true});node=lookup.node;path=lookup.path}var created=false;if(flags&64){if(node){if(flags&128){throw new FS.ErrnoError(20)}}else if(isDirPath){throw new FS.ErrnoError(31)}else{node=FS.mknod(path,mode|511,0);created=true}}if(!node){throw new FS.ErrnoError(44)}if(FS.isChrdev(node.mode)){flags&=~512}if(flags&65536&&!FS.isDir(node.mode)){throw new FS.ErrnoError(54)}if(!created){var errCode=FS.mayOpen(node,flags);if(errCode){throw new FS.ErrnoError(errCode)}}if(flags&512&&!created){FS.truncate(node,0)}flags&=~(128|512|131072);var stream=FS.createStream({node,path:FS.getPath(node),flags,seekable:true,position:0,stream_ops:node.stream_ops,ungotten:[],error:false});if(stream.stream_ops.open){stream.stream_ops.open(stream)}if(created){FS.chmod(node,mode&511)}if(Module["logReadFiles"]&&!(flags&1)){if(!(path in FS.readFiles)){FS.readFiles[path]=1}}return stream},close(stream){if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if(stream.getdents)stream.getdents=null;try{if(stream.stream_ops.close){stream.stream_ops.close(stream)}}catch(e){throw e}finally{FS.closeStream(stream.fd)}stream.fd=null},isClosed(stream){return stream.fd===null},llseek(stream,offset,whence){if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if(!stream.seekable||!stream.stream_ops.llseek){throw new FS.ErrnoError(70)}if(whence!=0&&whence!=1&&whence!=2){throw new FS.ErrnoError(28)}stream.position=stream.stream_ops.llseek(stream,offset,whence);stream.ungotten=[];return stream.position},read(stream,buffer,offset,length,position){if(length<0||position<0){throw new FS.ErrnoError(28)}if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if((stream.flags&2097155)===1){throw new FS.ErrnoError(8)}if(FS.isDir(stream.node.mode)){throw new FS.ErrnoError(31)}if(!stream.stream_ops.read){throw new FS.ErrnoError(28)}var seeking=typeof position!="undefined";if(!seeking){position=stream.position}else if(!stream.seekable){throw new FS.ErrnoError(70)}var bytesRead=stream.stream_ops.read(stream,buffer,offset,length,position);if(!seeking)stream.position+=bytesRead;return bytesRead},write(stream,buffer,offset,length,position,canOwn){if(length<0||position<0){throw new FS.ErrnoError(28)}if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if((stream.flags&2097155)===0){throw new FS.ErrnoError(8)}if(FS.isDir(stream.node.mode)){throw new FS.ErrnoError(31)}if(!stream.stream_ops.write){throw new FS.ErrnoError(28)}if(stream.seekable&&stream.flags&1024){FS.llseek(stream,0,2)}var seeking=typeof position!="undefined";if(!seeking){position=stream.position}else if(!stream.seekable){throw new FS.ErrnoError(70)}var bytesWritten=stream.stream_ops.write(stream,buffer,offset,length,position,canOwn);if(!seeking)stream.position+=bytesWritten;return bytesWritten},allocate(stream,offset,length){if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if(offset<0||length<=0){throw new FS.ErrnoError(28)}if((stream.flags&2097155)===0){throw new FS.ErrnoError(8)}if(!FS.isFile(stream.node.mode)&&!FS.isDir(stream.node.mode)){throw new FS.ErrnoError(43)}if(!stream.stream_ops.allocate){throw new FS.ErrnoError(138)}stream.stream_ops.allocate(stream,offset,length)},mmap(stream,length,position,prot,flags){if((prot&2)!==0&&(flags&2)===0&&(stream.flags&2097155)!==2){throw new FS.ErrnoError(2)}if((stream.flags&2097155)===1){throw new FS.ErrnoError(2)}if(!stream.stream_ops.mmap){throw new FS.ErrnoError(43)}if(!length){throw new FS.ErrnoError(28)}return stream.stream_ops.mmap(stream,length,position,prot,flags)},msync(stream,buffer,offset,length,mmapFlags){if(!stream.stream_ops.msync){return 0}return stream.stream_ops.msync(stream,buffer,offset,length,mmapFlags)},ioctl(stream,cmd,arg){if(!stream.stream_ops.ioctl){throw new FS.ErrnoError(59)}return stream.stream_ops.ioctl(stream,cmd,arg)},readFile(path,opts={}){opts.flags=opts.flags||0;opts.encoding=opts.encoding||"binary";if(opts.encoding!=="utf8"&&opts.encoding!=="binary"){throw new Error(`Invalid encoding type "${opts.encoding}"`)}var ret;var stream=FS.open(path,opts.flags);var stat=FS.stat(path);var length=stat.size;var buf=new Uint8Array(length);FS.read(stream,buf,0,length,0);if(opts.encoding==="utf8"){ret=UTF8ArrayToString(buf)}else if(opts.encoding==="binary"){ret=buf}FS.close(stream);return ret},writeFile(path,data,opts={}){opts.flags=opts.flags||577;var stream=FS.open(path,opts.flags,opts.mode);if(typeof data=="string"){var buf=new Uint8Array(lengthBytesUTF8(data)+1);var actualNumBytes=stringToUTF8Array(data,buf,0,buf.length);FS.write(stream,buf,0,actualNumBytes,undefined,opts.canOwn)}else if(ArrayBuffer.isView(data)){FS.write(stream,data,0,data.byteLength,undefined,opts.canOwn)}else{throw new Error("Unsupported data type")}FS.close(stream)},cwd:()=>FS.currentPath,chdir(path){var lookup=FS.lookupPath(path,{follow:true});if(lookup.node===null){throw new FS.ErrnoError(44)}if(!FS.isDir(lookup.node.mode)){throw new FS.ErrnoError(54)}var errCode=FS.nodePermissions(lookup.node,"x");if(errCode){throw new FS.ErrnoError(errCode)}FS.currentPath=lookup.path},createDefaultDirectories(){FS.mkdir("/tmp");FS.mkdir("/home");FS.mkdir("/home/web_user")},createDefaultDevices(){FS.mkdir("/dev");FS.registerDevice(FS.makedev(1,3),{read:()=>0,write:(stream,buffer,offset,length,pos)=>length,llseek:()=>0});FS.mkdev("/dev/null",FS.makedev(1,3));TTY.register(FS.makedev(5,0),TTY.default_tty_ops);TTY.register(FS.makedev(6,0),TTY.default_tty1_ops);FS.mkdev("/dev/tty",FS.makedev(5,0));FS.mkdev("/dev/tty1",FS.makedev(6,0));var randomBuffer=new Uint8Array(1024),randomLeft=0;var randomByte=()=>{if(randomLeft===0){randomFill(randomBuffer);randomLeft=randomBuffer.byteLength}return randomBuffer[--randomLeft]};FS.createDevice("/dev","random",randomByte);FS.createDevice("/dev","urandom",randomByte);FS.mkdir("/dev/shm");FS.mkdir("/dev/shm/tmp")},createSpecialDirectories(){FS.mkdir("/proc");var proc_self=FS.mkdir("/proc/self");FS.mkdir("/proc/self/fd");FS.mount({mount(){var node=FS.createNode(proc_self,"fd",16895,73);node.stream_ops={llseek:MEMFS.stream_ops.llseek};node.node_ops={lookup(parent,name){var fd=+name;var stream=FS.getStreamChecked(fd);var ret={parent:null,mount:{mountpoint:"fake"},node_ops:{readlink:()=>stream.path},id:fd+1};ret.parent=ret;return ret},readdir(){return Array.from(FS.streams.entries()).filter(([k,v])=>v).map(([k,v])=>k.toString())}};return node}},{},"/proc/self/fd")},createStandardStreams(input,output,error){if(input){FS.createDevice("/dev","stdin",input)}else{FS.symlink("/dev/tty","/dev/stdin")}if(output){FS.createDevice("/dev","stdout",null,output)}else{FS.symlink("/dev/tty","/dev/stdout")}if(error){FS.createDevice("/dev","stderr",null,error)}else{FS.symlink("/dev/tty1","/dev/stderr")}var stdin=FS.open("/dev/stdin",0);var stdout=FS.open("/dev/stdout",1);var stderr=FS.open("/dev/stderr",1)},staticInit(){FS.nameTable=new Array(4096);FS.mount(MEMFS,{},"/");FS.createDefaultDirectories();FS.createDefaultDevices();FS.createSpecialDirectories();FS.filesystems={MEMFS}},init(input,output,error){FS.initialized=true;input??=Module["stdin"];output??=Module["stdout"];error??=Module["stderr"];FS.createStandardStreams(input,output,error)},quit(){FS.initialized=false;for(var i=0;i<FS.streams.length;i++){var stream=FS.streams[i];if(!stream){continue}FS.close(stream)}},findObject(path,dontResolveLastLink){var ret=FS.analyzePath(path,dontResolveLastLink);if(!ret.exists){return null}return ret.object},analyzePath(path,dontResolveLastLink){try{var lookup=FS.lookupPath(path,{follow:!dontResolveLastLink});path=lookup.path}catch(e){}var ret={isRoot:false,exists:false,error:0,name:null,path:null,object:null,parentExists:false,parentPath:null,parentObject:null};try{var lookup=FS.lookupPath(path,{parent:true});ret.parentExists=true;ret.parentPath=lookup.path;ret.parentObject=lookup.node;ret.name=PATH.basename(path);lookup=FS.lookupPath(path,{follow:!dontResolveLastLink});ret.exists=true;ret.path=lookup.path;ret.object=lookup.node;ret.name=lookup.node.name;ret.isRoot=lookup.path==="/"}catch(e){ret.error=e.errno}return ret},createPath(parent,path,canRead,canWrite){parent=typeof parent=="string"?parent:FS.getPath(parent);var parts=path.split("/").reverse();while(parts.length){var part=parts.pop();if(!part)continue;var current=PATH.join2(parent,part);try{FS.mkdir(current)}catch(e){}parent=current}return current},createFile(parent,name,properties,canRead,canWrite){var path=PATH.join2(typeof parent=="string"?parent:FS.getPath(parent),name);var mode=FS_getMode(canRead,canWrite);return FS.create(path,mode)},createDataFile(parent,name,data,canRead,canWrite,canOwn){var path=name;if(parent){parent=typeof parent=="string"?parent:FS.getPath(parent);path=name?PATH.join2(parent,name):parent}var mode=FS_getMode(canRead,canWrite);var node=FS.create(path,mode);if(data){if(typeof data=="string"){var arr=new Array(data.length);for(var i=0,len=data.length;i<len;++i)arr[i]=data.charCodeAt(i);data=arr}FS.chmod(node,mode|146);var stream=FS.open(node,577);FS.write(stream,data,0,data.length,0,canOwn);FS.close(stream);FS.chmod(node,mode)}},createDevice(parent,name,input,output){var path=PATH.join2(typeof parent=="string"?parent:FS.getPath(parent),name);var mode=FS_getMode(!!input,!!output);FS.createDevice.major??=64;var dev=FS.makedev(FS.createDevice.major++,0);FS.registerDevice(dev,{open(stream){stream.seekable=false},close(stream){if(output?.buffer?.length){output(10)}},read(stream,buffer,offset,length,pos){var bytesRead=0;for(var i=0;i<length;i++){var result;try{result=input()}catch(e){throw new FS.ErrnoError(29)}if(result===undefined&&bytesRead===0){throw new FS.ErrnoError(6)}if(result===null||result===undefined)break;bytesRead++;buffer[offset+i]=result}if(bytesRead){stream.node.atime=Date.now()}return bytesRead},write(stream,buffer,offset,length,pos){for(var i=0;i<length;i++){try{output(buffer[offset+i])}catch(e){throw new FS.ErrnoError(29)}}if(length){stream.node.mtime=stream.node.ctime=Date.now()}return i}});return FS.mkdev(path,mode,dev)},forceLoadFile(obj){if(obj.isDevice||obj.isFolder||obj.link||obj.contents)return true;if(typeof XMLHttpRequest!="undefined"){throw new Error("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.")}else{try{obj.contents=readBinary(obj.url);obj.usedBytes=obj.contents.length}catch(e){throw new FS.ErrnoError(29)}}},createLazyFile(parent,name,url,canRead,canWrite){class LazyUint8Array{lengthKnown=false;chunks=[];get(idx){if(idx>this.length-1||idx<0){return undefined}var chunkOffset=idx%this.chunkSize;var chunkNum=idx/this.chunkSize|0;return this.getter(chunkNum)[chunkOffset]}setDataGetter(getter){this.getter=getter}cacheLength(){var xhr=new XMLHttpRequest;xhr.open("HEAD",url,false);xhr.send(null);if(!(xhr.status>=200&&xhr.status<300||xhr.status===304))throw new Error("Couldn\'t load "+url+". Status: "+xhr.status);var datalength=Number(xhr.getResponseHeader("Content-length"));var header;var hasByteServing=(header=xhr.getResponseHeader("Accept-Ranges"))&&header==="bytes";var usesGzip=(header=xhr.getResponseHeader("Content-Encoding"))&&header==="gzip";var chunkSize=1024*1024;if(!hasByteServing)chunkSize=datalength;var doXHR=(from,to)=>{if(from>to)throw new Error("invalid range ("+from+", "+to+") or no bytes requested!");if(to>datalength-1)throw new Error("only "+datalength+" bytes available! programmer error!");var xhr=new XMLHttpRequest;xhr.open("GET",url,false);if(datalength!==chunkSize)xhr.setRequestHeader("Range","bytes="+from+"-"+to);xhr.responseType="arraybuffer";if(xhr.overrideMimeType){xhr.overrideMimeType("text/plain; charset=x-user-defined")}xhr.send(null);if(!(xhr.status>=200&&xhr.status<300||xhr.status===304))throw new Error("Couldn\'t load "+url+". Status: "+xhr.status);if(xhr.response!==undefined){return new Uint8Array(xhr.response||[])}return intArrayFromString(xhr.responseText||"",true)};var lazyArray=this;lazyArray.setDataGetter(chunkNum=>{var start=chunkNum*chunkSize;var end=(chunkNum+1)*chunkSize-1;end=Math.min(end,datalength-1);if(typeof lazyArray.chunks[chunkNum]=="undefined"){lazyArray.chunks[chunkNum]=doXHR(start,end)}if(typeof lazyArray.chunks[chunkNum]=="undefined")throw new Error("doXHR failed!");return lazyArray.chunks[chunkNum]});if(usesGzip||!datalength){chunkSize=datalength=1;datalength=this.getter(0).length;chunkSize=datalength;out("LazyFiles on gzip forces download of the whole file when length is accessed")}this._length=datalength;this._chunkSize=chunkSize;this.lengthKnown=true}get length(){if(!this.lengthKnown){this.cacheLength()}return this._length}get chunkSize(){if(!this.lengthKnown){this.cacheLength()}return this._chunkSize}}if(typeof XMLHttpRequest!="undefined"){if(!ENVIRONMENT_IS_WORKER)throw"Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc";var lazyArray=new LazyUint8Array;var properties={isDevice:false,contents:lazyArray}}else{var properties={isDevice:false,url}}var node=FS.createFile(parent,name,properties,canRead,canWrite);if(properties.contents){node.contents=properties.contents}else if(properties.url){node.contents=null;node.url=properties.url}Object.defineProperties(node,{usedBytes:{get:function(){return this.contents.length}}});var stream_ops={};var keys=Object.keys(node.stream_ops);keys.forEach(key=>{var fn=node.stream_ops[key];stream_ops[key]=(...args)=>{FS.forceLoadFile(node);return fn(...args)}});function writeChunks(stream,buffer,offset,length,position){var contents=stream.node.contents;if(position>=contents.length)return 0;var size=Math.min(contents.length-position,length);if(contents.slice){for(var i=0;i<size;i++){buffer[offset+i]=contents[position+i]}}else{for(var i=0;i<size;i++){buffer[offset+i]=contents.get(position+i)}}return size}stream_ops.read=(stream,buffer,offset,length,position)=>{FS.forceLoadFile(node);return writeChunks(stream,buffer,offset,length,position)};stream_ops.mmap=(stream,length,position,prot,flags)=>{FS.forceLoadFile(node);var ptr=mmapAlloc(length);if(!ptr){throw new FS.ErrnoError(48)}writeChunks(stream,GROWABLE_HEAP_I8(),ptr,length,position);return{ptr,allocated:true}};node.stream_ops=stream_ops;return node}};Module["FS"]=FS;var UTF8ToString=(ptr,maxBytesToRead)=>{ptr>>>=0;return ptr?UTF8ArrayToString(GROWABLE_HEAP_U8(),ptr,maxBytesToRead):""};Module["UTF8ToString"]=UTF8ToString;var SYSCALLS={DEFAULT_POLLMASK:5,calculateAt(dirfd,path,allowEmpty){if(PATH.isAbs(path)){return path}var dir;if(dirfd===-100){dir=FS.cwd()}else{var dirstream=SYSCALLS.getStreamFromFD(dirfd);dir=dirstream.path}if(path.length==0){if(!allowEmpty){throw new FS.ErrnoError(44)}return dir}return dir+"/"+path},writeStat(buf,stat){GROWABLE_HEAP_I32()[buf>>>2>>>0]=stat.dev;GROWABLE_HEAP_I32()[buf+4>>>2>>>0]=stat.mode;GROWABLE_HEAP_U32()[buf+8>>>2>>>0]=stat.nlink;GROWABLE_HEAP_I32()[buf+12>>>2>>>0]=stat.uid;GROWABLE_HEAP_I32()[buf+16>>>2>>>0]=stat.gid;GROWABLE_HEAP_I32()[buf+20>>>2>>>0]=stat.rdev;HEAP64[buf+24>>>3]=BigInt(stat.size);GROWABLE_HEAP_I32()[buf+32>>>2>>>0]=4096;GROWABLE_HEAP_I32()[buf+36>>>2>>>0]=stat.blocks;var atime=stat.atime.getTime();var mtime=stat.mtime.getTime();var ctime=stat.ctime.getTime();HEAP64[buf+40>>>3]=BigInt(Math.floor(atime/1e3));GROWABLE_HEAP_U32()[buf+48>>>2>>>0]=atime%1e3*1e3*1e3;HEAP64[buf+56>>>3]=BigInt(Math.floor(mtime/1e3));GROWABLE_HEAP_U32()[buf+64>>>2>>>0]=mtime%1e3*1e3*1e3;HEAP64[buf+72>>>3]=BigInt(Math.floor(ctime/1e3));GROWABLE_HEAP_U32()[buf+80>>>2>>>0]=ctime%1e3*1e3*1e3;HEAP64[buf+88>>>3]=BigInt(stat.ino);return 0},doMsync(addr,stream,len,flags,offset){if(!FS.isFile(stream.node.mode)){throw new FS.ErrnoError(43)}if(flags&2){return 0}var buffer=GROWABLE_HEAP_U8().slice(addr,addr+len);FS.msync(stream,buffer,offset,len,flags)},getStreamFromFD(fd){var stream=FS.getStreamChecked(fd);return stream},varargs:undefined,getStr(ptr){var ret=UTF8ToString(ptr);return ret}};Module["SYSCALLS"]=SYSCALLS;function ___syscall_fcntl64(fd,cmd,varargs){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(3,0,1,fd,cmd,varargs);varargs>>>=0;SYSCALLS.varargs=varargs;try{var stream=SYSCALLS.getStreamFromFD(fd);switch(cmd){case 0:{var arg=syscallGetVarargI();if(arg<0){return-28}while(FS.streams[arg]){arg++}var newStream;newStream=FS.dupStream(stream,arg);return newStream.fd}case 1:case 2:return 0;case 3:return stream.flags;case 4:{var arg=syscallGetVarargI();stream.flags|=arg;return 0}case 12:{var arg=syscallGetVarargP();var offset=0;GROWABLE_HEAP_I16()[arg+offset>>>1>>>0]=2;return 0}case 13:case 14:return 0}return-28}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["___syscall_fcntl64"]=___syscall_fcntl64;function ___syscall_ioctl(fd,op,varargs){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(4,0,1,fd,op,varargs);varargs>>>=0;SYSCALLS.varargs=varargs;try{var stream=SYSCALLS.getStreamFromFD(fd);switch(op){case 21509:{if(!stream.tty)return-59;return 0}case 21505:{if(!stream.tty)return-59;if(stream.tty.ops.ioctl_tcgets){var termios=stream.tty.ops.ioctl_tcgets(stream);var argp=syscallGetVarargP();GROWABLE_HEAP_I32()[argp>>>2>>>0]=termios.c_iflag||0;GROWABLE_HEAP_I32()[argp+4>>>2>>>0]=termios.c_oflag||0;GROWABLE_HEAP_I32()[argp+8>>>2>>>0]=termios.c_cflag||0;GROWABLE_HEAP_I32()[argp+12>>>2>>>0]=termios.c_lflag||0;for(var i=0;i<32;i++){GROWABLE_HEAP_I8()[argp+i+17>>>0]=termios.c_cc[i]||0}return 0}return 0}case 21510:case 21511:case 21512:{if(!stream.tty)return-59;return 0}case 21506:case 21507:case 21508:{if(!stream.tty)return-59;if(stream.tty.ops.ioctl_tcsets){var argp=syscallGetVarargP();var c_iflag=GROWABLE_HEAP_I32()[argp>>>2>>>0];var c_oflag=GROWABLE_HEAP_I32()[argp+4>>>2>>>0];var c_cflag=GROWABLE_HEAP_I32()[argp+8>>>2>>>0];var c_lflag=GROWABLE_HEAP_I32()[argp+12>>>2>>>0];var c_cc=[];for(var i=0;i<32;i++){c_cc.push(GROWABLE_HEAP_I8()[argp+i+17>>>0])}return stream.tty.ops.ioctl_tcsets(stream.tty,op,{c_iflag,c_oflag,c_cflag,c_lflag,c_cc})}return 0}case 21519:{if(!stream.tty)return-59;var argp=syscallGetVarargP();GROWABLE_HEAP_I32()[argp>>>2>>>0]=0;return 0}case 21520:{if(!stream.tty)return-59;return-28}case 21531:{var argp=syscallGetVarargP();return FS.ioctl(stream,op,argp)}case 21523:{if(!stream.tty)return-59;if(stream.tty.ops.ioctl_tiocgwinsz){var winsize=stream.tty.ops.ioctl_tiocgwinsz(stream.tty);var argp=syscallGetVarargP();GROWABLE_HEAP_I16()[argp>>>1>>>0]=winsize[0];GROWABLE_HEAP_I16()[argp+2>>>1>>>0]=winsize[1]}return 0}case 21524:{if(!stream.tty)return-59;return 0}case 21515:{if(!stream.tty)return-59;return 0}default:return-28}}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["___syscall_ioctl"]=___syscall_ioctl;function ___syscall_openat(dirfd,path,flags,varargs){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(5,0,1,dirfd,path,flags,varargs);path>>>=0;varargs>>>=0;SYSCALLS.varargs=varargs;try{path=SYSCALLS.getStr(path);path=SYSCALLS.calculateAt(dirfd,path);var mode=varargs?syscallGetVarargI():0;return FS.open(path,flags,mode).fd}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["___syscall_openat"]=___syscall_openat;var __abort_js=()=>abort("");Module["__abort_js"]=__abort_js;function __emscripten_init_main_thread_js(tb){tb>>>=0;__emscripten_thread_init(tb,!ENVIRONMENT_IS_WORKER,1,!ENVIRONMENT_IS_WEB,65536,false);PThread.threadInitTLS()}Module["__emscripten_init_main_thread_js"]=__emscripten_init_main_thread_js;var maybeExit=()=>{if(!keepRuntimeAlive()){try{if(ENVIRONMENT_IS_PTHREAD)__emscripten_thread_exit(EXITSTATUS);else _exit(EXITSTATUS)}catch(e){handleException(e)}}};Module["maybeExit"]=maybeExit;var callUserCallback=func=>{if(ABORT){return}try{func();maybeExit()}catch(e){handleException(e)}};Module["callUserCallback"]=callUserCallback;function __emscripten_thread_mailbox_await(pthread_ptr){pthread_ptr>>>=0;if(typeof Atomics.waitAsync==="function"){var wait=Atomics.waitAsync(GROWABLE_HEAP_I32(),pthread_ptr>>>2,pthread_ptr);wait.value.then(checkMailbox);var waitingAsync=pthread_ptr+128;Atomics.store(GROWABLE_HEAP_I32(),waitingAsync>>>2,1)}}Module["__emscripten_thread_mailbox_await"]=__emscripten_thread_mailbox_await;var checkMailbox=()=>{var pthread_ptr=_pthread_self();if(pthread_ptr){__emscripten_thread_mailbox_await(pthread_ptr);callUserCallback(__emscripten_check_mailbox)}};Module["checkMailbox"]=checkMailbox;function __emscripten_notify_mailbox_postmessage(targetThread,currThreadId){targetThread>>>=0;currThreadId>>>=0;if(targetThread==currThreadId){setTimeout(checkMailbox)}else if(ENVIRONMENT_IS_PTHREAD){postMessage({targetThread,cmd:"checkMailbox"})}else{var worker=PThread.pthreads[targetThread];if(!worker){return}worker.postMessage({cmd:"checkMailbox"})}}Module["__emscripten_notify_mailbox_postmessage"]=__emscripten_notify_mailbox_postmessage;var proxiedJSCallArgs=[];Module["proxiedJSCallArgs"]=proxiedJSCallArgs;function __emscripten_receive_on_main_thread_js(funcIndex,emAsmAddr,callingThread,numCallArgs,args){emAsmAddr>>>=0;callingThread>>>=0;args>>>=0;numCallArgs/=2;proxiedJSCallArgs.length=numCallArgs;var b=args>>>3;for(var i=0;i<numCallArgs;i++){if(HEAP64[b+2*i]){proxiedJSCallArgs[i]=HEAP64[b+2*i+1]}else{proxiedJSCallArgs[i]=GROWABLE_HEAP_F64()[b+2*i+1>>>0]}}var func=proxiedFunctionTable[funcIndex];PThread.currentProxiedOperationCallerThread=callingThread;var rtn=func(...proxiedJSCallArgs);PThread.currentProxiedOperationCallerThread=0;return rtn}Module["__emscripten_receive_on_main_thread_js"]=__emscripten_receive_on_main_thread_js;var __emscripten_runtime_keepalive_clear=()=>{noExitRuntime=false;runtimeKeepaliveCounter=0};Module["__emscripten_runtime_keepalive_clear"]=__emscripten_runtime_keepalive_clear;function __emscripten_thread_cleanup(thread){thread>>>=0;if(!ENVIRONMENT_IS_PTHREAD)cleanupThread(thread);else postMessage({cmd:"cleanupThread",thread})}Module["__emscripten_thread_cleanup"]=__emscripten_thread_cleanup;function __emscripten_thread_set_strongref(thread){thread>>>=0;if(ENVIRONMENT_IS_NODE){PThread.pthreads[thread].ref()}}Module["__emscripten_thread_set_strongref"]=__emscripten_thread_set_strongref;function __mmap_js(len,prot,flags,fd,offset,allocated,addr){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(6,0,1,len,prot,flags,fd,offset,allocated,addr);len>>>=0;offset=bigintToI53Checked(offset);allocated>>>=0;addr>>>=0;try{if(isNaN(offset))return 61;var stream=SYSCALLS.getStreamFromFD(fd);var res=FS.mmap(stream,len,offset,prot,flags);var ptr=res.ptr;GROWABLE_HEAP_I32()[allocated>>>2>>>0]=res.allocated;GROWABLE_HEAP_U32()[addr>>>2>>>0]=ptr;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["__mmap_js"]=__mmap_js;function __munmap_js(addr,len,prot,flags,fd,offset){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(7,0,1,addr,len,prot,flags,fd,offset);addr>>>=0;len>>>=0;offset=bigintToI53Checked(offset);try{var stream=SYSCALLS.getStreamFromFD(fd);if(prot&2){SYSCALLS.doMsync(addr,stream,len,flags,offset)}}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["__munmap_js"]=__munmap_js;var timers={};Module["timers"]=timers;var _emscripten_get_now=()=>performance.timeOrigin+performance.now();Module["_emscripten_get_now"]=_emscripten_get_now;function __setitimer_js(which,timeout_ms){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(8,0,1,which,timeout_ms);if(timers[which]){clearTimeout(timers[which].id);delete timers[which]}if(!timeout_ms)return 0;var id=setTimeout(()=>{delete timers[which];callUserCallback(()=>__emscripten_timeout(which,_emscripten_get_now()))},timeout_ms);timers[which]={id,timeout_ms};return 0}Module["__setitimer_js"]=__setitimer_js;var stringToUTF8=(str,outPtr,maxBytesToWrite)=>stringToUTF8Array(str,GROWABLE_HEAP_U8(),outPtr,maxBytesToWrite);Module["stringToUTF8"]=stringToUTF8;var __tzset_js=function(timezone,daylight,std_name,dst_name){timezone>>>=0;daylight>>>=0;std_name>>>=0;dst_name>>>=0;var currentYear=(new Date).getFullYear();var winter=new Date(currentYear,0,1);var summer=new Date(currentYear,6,1);var winterOffset=winter.getTimezoneOffset();var summerOffset=summer.getTimezoneOffset();var stdTimezoneOffset=Math.max(winterOffset,summerOffset);GROWABLE_HEAP_U32()[timezone>>>2>>>0]=stdTimezoneOffset*60;GROWABLE_HEAP_I32()[daylight>>>2>>>0]=Number(winterOffset!=summerOffset);var extractZone=timezoneOffset=>{var sign=timezoneOffset>=0?"-":"+";var absOffset=Math.abs(timezoneOffset);var hours=String(Math.floor(absOffset/60)).padStart(2,"0");var minutes=String(absOffset%60).padStart(2,"0");return`UTC${sign}${hours}${minutes}`};var winterName=extractZone(winterOffset);var summerName=extractZone(summerOffset);if(summerOffset<winterOffset){stringToUTF8(winterName,std_name,17);stringToUTF8(summerName,dst_name,17)}else{stringToUTF8(winterName,dst_name,17);stringToUTF8(summerName,std_name,17)}};Module["__tzset_js"]=__tzset_js;var _emscripten_date_now=()=>Date.now();Module["_emscripten_date_now"]=_emscripten_date_now;var nowIsMonotonic=1;Module["nowIsMonotonic"]=nowIsMonotonic;var checkWasiClock=clock_id=>clock_id>=0&&clock_id<=3;Module["checkWasiClock"]=checkWasiClock;function _clock_time_get(clk_id,ignored_precision,ptime){ignored_precision=bigintToI53Checked(ignored_precision);ptime>>>=0;if(!checkWasiClock(clk_id)){return 28}var now;if(clk_id===0){now=_emscripten_date_now()}else if(nowIsMonotonic){now=_emscripten_get_now()}else{return 52}var nsec=Math.round(now*1e3*1e3);HEAP64[ptime>>>3]=BigInt(nsec);return 0}Module["_clock_time_get"]=_clock_time_get;var warnOnce=text=>{warnOnce.shown||={};if(!warnOnce.shown[text]){warnOnce.shown[text]=1;if(ENVIRONMENT_IS_NODE)text="warning: "+text;err(text)}};Module["warnOnce"]=warnOnce;var _emscripten_check_blocking_allowed=()=>{};Module["_emscripten_check_blocking_allowed"]=_emscripten_check_blocking_allowed;var runtimeKeepalivePush=()=>{runtimeKeepaliveCounter+=1};Module["runtimeKeepalivePush"]=runtimeKeepalivePush;var _emscripten_exit_with_live_runtime=()=>{runtimeKeepalivePush();throw"unwind"};Module["_emscripten_exit_with_live_runtime"]=_emscripten_exit_with_live_runtime;var getHeapMax=()=>4294901760;Module["getHeapMax"]=getHeapMax;var growMemory=size=>{var b=wasmMemory.buffer;var pages=(size-b.byteLength+65535)/65536|0;try{wasmMemory.grow(pages);updateMemoryViews();return 1}catch(e){}};Module["growMemory"]=growMemory;function _emscripten_resize_heap(requestedSize){requestedSize>>>=0;var oldSize=GROWABLE_HEAP_U8().length;if(requestedSize<=oldSize){return false}var maxHeapSize=getHeapMax();if(requestedSize>maxHeapSize){return false}for(var cutDown=1;cutDown<=4;cutDown*=2){var overGrownHeapSize=oldSize*(1+.2/cutDown);overGrownHeapSize=Math.min(overGrownHeapSize,requestedSize+100663296);var newSize=Math.min(maxHeapSize,alignMemory(Math.max(requestedSize,overGrownHeapSize),65536));var replacement=growMemory(newSize);if(replacement){return true}}return false}Module["_emscripten_resize_heap"]=_emscripten_resize_heap;var ENV={};Module["ENV"]=ENV;var getExecutableName=()=>thisProgram||"./this.program";Module["getExecutableName"]=getExecutableName;var getEnvStrings=()=>{if(!getEnvStrings.strings){var lang=(typeof navigator=="object"&&navigator.languages&&navigator.languages[0]||"C").replace("-","_")+".UTF-8";var env={USER:"web_user",LOGNAME:"web_user",PATH:"/",PWD:"/",HOME:"/home/web_user",LANG:lang,_:getExecutableName()};for(var x in ENV){if(ENV[x]===undefined)delete env[x];else env[x]=ENV[x]}var strings=[];for(var x in env){strings.push(`${x}=${env[x]}`)}getEnvStrings.strings=strings}return getEnvStrings.strings};Module["getEnvStrings"]=getEnvStrings;var stringToAscii=(str,buffer)=>{for(var i=0;i<str.length;++i){GROWABLE_HEAP_I8()[buffer++>>>0]=str.charCodeAt(i)}GROWABLE_HEAP_I8()[buffer>>>0]=0};Module["stringToAscii"]=stringToAscii;var _environ_get=function(__environ,environ_buf){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(9,0,1,__environ,environ_buf);__environ>>>=0;environ_buf>>>=0;var bufSize=0;getEnvStrings().forEach((string,i)=>{var ptr=environ_buf+bufSize;GROWABLE_HEAP_U32()[__environ+i*4>>>2>>>0]=ptr;stringToAscii(string,ptr);bufSize+=string.length+1});return 0};Module["_environ_get"]=_environ_get;var _environ_sizes_get=function(penviron_count,penviron_buf_size){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(10,0,1,penviron_count,penviron_buf_size);penviron_count>>>=0;penviron_buf_size>>>=0;var strings=getEnvStrings();GROWABLE_HEAP_U32()[penviron_count>>>2>>>0]=strings.length;var bufSize=0;strings.forEach(string=>bufSize+=string.length+1);GROWABLE_HEAP_U32()[penviron_buf_size>>>2>>>0]=bufSize;return 0};Module["_environ_sizes_get"]=_environ_sizes_get;function _fd_close(fd){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(11,0,1,fd);try{var stream=SYSCALLS.getStreamFromFD(fd);FS.close(stream);return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_close"]=_fd_close;var doReadv=(stream,iov,iovcnt,offset)=>{var ret=0;for(var i=0;i<iovcnt;i++){var ptr=GROWABLE_HEAP_U32()[iov>>>2>>>0];var len=GROWABLE_HEAP_U32()[iov+4>>>2>>>0];iov+=8;var curr=FS.read(stream,GROWABLE_HEAP_I8(),ptr,len,offset);if(curr<0)return-1;ret+=curr;if(curr<len)break;if(typeof offset!="undefined"){offset+=curr}}return ret};Module["doReadv"]=doReadv;function _fd_read(fd,iov,iovcnt,pnum){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(12,0,1,fd,iov,iovcnt,pnum);iov>>>=0;iovcnt>>>=0;pnum>>>=0;try{var stream=SYSCALLS.getStreamFromFD(fd);var num=doReadv(stream,iov,iovcnt);GROWABLE_HEAP_U32()[pnum>>>2>>>0]=num;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_read"]=_fd_read;function _fd_seek(fd,offset,whence,newOffset){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(13,0,1,fd,offset,whence,newOffset);offset=bigintToI53Checked(offset);newOffset>>>=0;try{if(isNaN(offset))return 61;var stream=SYSCALLS.getStreamFromFD(fd);FS.llseek(stream,offset,whence);HEAP64[newOffset>>>3]=BigInt(stream.position);if(stream.getdents&&offset===0&&whence===0)stream.getdents=null;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_seek"]=_fd_seek;var doWritev=(stream,iov,iovcnt,offset)=>{var ret=0;for(var i=0;i<iovcnt;i++){var ptr=GROWABLE_HEAP_U32()[iov>>>2>>>0];var len=GROWABLE_HEAP_U32()[iov+4>>>2>>>0];iov+=8;var curr=FS.write(stream,GROWABLE_HEAP_I8(),ptr,len,offset);if(curr<0)return-1;ret+=curr;if(curr<len){break}if(typeof offset!="undefined"){offset+=curr}}return ret};Module["doWritev"]=doWritev;function _fd_write(fd,iov,iovcnt,pnum){if(ENVIRONMENT_IS_PTHREAD)return proxyToMainThread(14,0,1,fd,iov,iovcnt,pnum);iov>>>=0;iovcnt>>>=0;pnum>>>=0;try{var stream=SYSCALLS.getStreamFromFD(fd);var num=doWritev(stream,iov,iovcnt);GROWABLE_HEAP_U32()[pnum>>>2>>>0]=num;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_write"]=_fd_write;function _random_get(buffer,size){buffer>>>=0;size>>>=0;try{randomFill(GROWABLE_HEAP_U8().subarray(buffer>>>0,buffer+size>>>0));return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_random_get"]=_random_get;var getCFunc=ident=>{var func=Module["_"+ident];return func};Module["getCFunc"]=getCFunc;var writeArrayToMemory=(array,buffer)=>{GROWABLE_HEAP_I8().set(array,buffer>>>0)};Module["writeArrayToMemory"]=writeArrayToMemory;var stringToUTF8OnStack=str=>{var size=lengthBytesUTF8(str)+1;var ret=stackAlloc(size);stringToUTF8(str,ret,size);return ret};Module["stringToUTF8OnStack"]=stringToUTF8OnStack;var ccall=(ident,returnType,argTypes,args,opts)=>{var toC={string:str=>{var ret=0;if(str!==null&&str!==undefined&&str!==0){ret=stringToUTF8OnStack(str)}return ret},array:arr=>{var ret=stackAlloc(arr.length);writeArrayToMemory(arr,ret);return ret}};function convertReturnValue(ret){if(returnType==="string"){return UTF8ToString(ret)}if(returnType==="boolean")return Boolean(ret);return ret}var func=getCFunc(ident);var cArgs=[];var stack=0;if(args){for(var i=0;i<args.length;i++){var converter=toC[argTypes[i]];if(converter){if(stack===0)stack=stackSave();cArgs[i]=converter(args[i])}else{cArgs[i]=args[i]}}}var ret=func(...cArgs);function onDone(ret){if(stack!==0)stackRestore(stack);return convertReturnValue(ret)}ret=onDone(ret);return ret};Module["ccall"]=ccall;var cwrap=(ident,returnType,argTypes,opts)=>{var numericArgs=!argTypes||argTypes.every(type=>type==="number"||type==="boolean");var numericRet=returnType!=="string";if(numericRet&&numericArgs&&!opts){return getCFunc(ident)}return(...args)=>ccall(ident,returnType,argTypes,args,opts)};Module["cwrap"]=cwrap;var FS_createPath=FS.createPath;Module["FS_createPath"]=FS_createPath;var FS_unlink=path=>FS.unlink(path);Module["FS_unlink"]=FS_unlink;var FS_createLazyFile=FS.createLazyFile;Module["FS_createLazyFile"]=FS_createLazyFile;var FS_createDevice=FS.createDevice;Module["FS_createDevice"]=FS_createDevice;PThread.init();FS.createPreloadedFile=FS_createPreloadedFile;FS.staticInit();Module["FS_createPath"]=FS.createPath;Module["FS_createDataFile"]=FS.createDataFile;Module["FS_createPreloadedFile"]=FS.createPreloadedFile;Module["FS_unlink"]=FS.unlink;Module["FS_createLazyFile"]=FS.createLazyFile;Module["FS_createDevice"]=FS.createDevice;MEMFS.doesNotExistError=new FS.ErrnoError(44);MEMFS.doesNotExistError.stack="<generic error, no stack>";var proxiedFunctionTable=[_proc_exit,exitOnMainThread,pthreadCreateProxied,___syscall_fcntl64,___syscall_ioctl,___syscall_openat,__mmap_js,__munmap_js,__setitimer_js,_environ_get,_environ_sizes_get,_fd_close,_fd_read,_fd_seek,_fd_write];var wasmImports;function assignWasmImports(){wasmImports={s:___pthread_create_js,c:___syscall_fcntl64,h:___syscall_ioctl,i:___syscall_openat,F:__abort_js,A:__emscripten_init_main_thread_js,q:__emscripten_notify_mailbox_postmessage,t:__emscripten_receive_on_main_thread_js,m:__emscripten_runtime_keepalive_clear,d:__emscripten_thread_cleanup,z:__emscripten_thread_mailbox_await,l:__emscripten_thread_set_strongref,v:__mmap_js,w:__munmap_js,n:__setitimer_js,x:__tzset_js,E:_clock_time_get,e:_emscripten_check_blocking_allowed,u:_emscripten_date_now,j:_emscripten_exit_with_live_runtime,b:_emscripten_get_now,p:_emscripten_resize_heap,B:_environ_get,C:_environ_sizes_get,r:_exit,f:_fd_close,g:_fd_read,y:_fd_seek,D:_fd_write,a:wasmMemory,k:_proc_exit,o:_random_get}}var wasmExports;createWasm();var ___wasm_call_ctors=()=>(___wasm_call_ctors=wasmExports["G"])();var _wllama_malloc=Module["_wllama_malloc"]=(a0,a1)=>(_wllama_malloc=Module["_wllama_malloc"]=wasmExports["H"])(a0,a1);var _wllama_start=Module["_wllama_start"]=()=>(_wllama_start=Module["_wllama_start"]=wasmExports["I"])();var _wllama_action=Module["_wllama_action"]=(a0,a1)=>(_wllama_action=Module["_wllama_action"]=wasmExports["J"])(a0,a1);var _wllama_exit=Module["_wllama_exit"]=()=>(_wllama_exit=Module["_wllama_exit"]=wasmExports["K"])();var _wllama_debug=Module["_wllama_debug"]=()=>(_wllama_debug=Module["_wllama_debug"]=wasmExports["L"])();var _main=Module["_main"]=(a0,a1)=>(_main=Module["_main"]=wasmExports["M"])(a0,a1);var __emscripten_tls_init=()=>(__emscripten_tls_init=wasmExports["N"])();var _pthread_self=()=>(_pthread_self=wasmExports["O"])();var _emscripten_builtin_memalign=(a0,a1)=>(_emscripten_builtin_memalign=wasmExports["P"])(a0,a1);var __emscripten_thread_init=(a0,a1,a2,a3,a4,a5)=>(__emscripten_thread_init=wasmExports["R"])(a0,a1,a2,a3,a4,a5);var __emscripten_thread_crashed=()=>(__emscripten_thread_crashed=wasmExports["S"])();var __emscripten_run_on_main_thread_js=(a0,a1,a2,a3,a4)=>(__emscripten_run_on_main_thread_js=wasmExports["T"])(a0,a1,a2,a3,a4);var __emscripten_thread_free_data=a0=>(__emscripten_thread_free_data=wasmExports["U"])(a0);var __emscripten_thread_exit=a0=>(__emscripten_thread_exit=wasmExports["V"])(a0);var __emscripten_timeout=(a0,a1)=>(__emscripten_timeout=wasmExports["W"])(a0,a1);var __emscripten_check_mailbox=()=>(__emscripten_check_mailbox=wasmExports["X"])();var ___trap=()=>(___trap=wasmExports["Y"])();var _emscripten_stack_set_limits=(a0,a1)=>(_emscripten_stack_set_limits=wasmExports["Z"])(a0,a1);var __emscripten_stack_restore=a0=>(__emscripten_stack_restore=wasmExports["_"])(a0);var __emscripten_stack_alloc=a0=>(__emscripten_stack_alloc=wasmExports["$"])(a0);var _emscripten_stack_get_current=()=>(_emscripten_stack_get_current=wasmExports["aa"])();function applySignatureConversions(wasmExports){wasmExports=Object.assign({},wasmExports);var makeWrapper_p=f=>()=>f()>>>0;var makeWrapper_ppp=f=>(a0,a1)=>f(a0,a1)>>>0;var makeWrapper_pp=f=>a0=>f(a0)>>>0;wasmExports["O"]=makeWrapper_p(wasmExports["O"]);wasmExports["P"]=makeWrapper_ppp(wasmExports["P"]);wasmExports["$"]=makeWrapper_pp(wasmExports["$"]);wasmExports["aa"]=makeWrapper_p(wasmExports["aa"]);return wasmExports}Module["addRunDependency"]=addRunDependency;Module["removeRunDependency"]=removeRunDependency;Module["ccall"]=ccall;Module["cwrap"]=cwrap;Module["FS_createPreloadedFile"]=FS_createPreloadedFile;Module["FS_unlink"]=FS_unlink;Module["FS_createPath"]=FS_createPath;Module["FS_createDevice"]=FS_createDevice;Module["FS_createDataFile"]=FS_createDataFile;Module["FS_createLazyFile"]=FS_createLazyFile;function callMain(){var entryFunction=_main;var argc=0;var argv=0;try{var ret=entryFunction(argc,argv);exitJS(ret,true);return ret}catch(e){return handleException(e)}}function run(){if(runDependencies>0){dependenciesFulfilled=run;return}if(ENVIRONMENT_IS_PTHREAD){initRuntime();return}preRun();if(runDependencies>0){dependenciesFulfilled=run;return}function doRun(){Module["calledRun"]=true;if(ABORT)return;initRuntime();preMain();Module["onRuntimeInitialized"]?.();var noInitialRun=Module["noInitialRun"];if(!noInitialRun)callMain();postRun()}if(Module["setStatus"]){Module["setStatus"]("Running...");setTimeout(()=>{setTimeout(()=>Module["setStatus"](""),1);doRun()},1)}else{doRun()}}if(Module["preInit"]){if(typeof Module["preInit"]=="function")Module["preInit"]=[Module["preInit"]];while(Module["preInit"].length>0){Module["preInit"].pop()()}}run();\n';
var WLLAMA_SINGLE_THREAD_CODE = 'var Module=typeof Module!="undefined"?Module:{};var ENVIRONMENT_IS_WEB=typeof window=="object";var ENVIRONMENT_IS_WORKER=typeof WorkerGlobalScope!="undefined";var ENVIRONMENT_IS_NODE=typeof process=="object"&&typeof process.versions=="object"&&typeof process.versions.node=="string"&&process.type!="renderer";if(ENVIRONMENT_IS_NODE){}var moduleOverrides=Object.assign({},Module);var arguments_=[];var thisProgram="./this.program";var quit_=(status,toThrow)=>{throw toThrow};var scriptDirectory="";function locateFile(path){if(Module["locateFile"]){return Module["locateFile"](path,scriptDirectory)}return scriptDirectory+path}var readAsync,readBinary;if(ENVIRONMENT_IS_NODE){var fs=require("fs");var nodePath=require("path");scriptDirectory=__dirname+"/";readBinary=filename=>{filename=isFileURI(filename)?new URL(filename):filename;var ret=fs.readFileSync(filename);return ret};readAsync=async(filename,binary=true)=>{filename=isFileURI(filename)?new URL(filename):filename;var ret=fs.readFileSync(filename,binary?undefined:"utf8");return ret};if(!Module["thisProgram"]&&process.argv.length>1){thisProgram=process.argv[1].replace(/\\\\/g,"/")}arguments_=process.argv.slice(2);if(typeof module!="undefined"){module["exports"]=Module}quit_=(status,toThrow)=>{process.exitCode=status;throw toThrow}}else if(ENVIRONMENT_IS_WEB||ENVIRONMENT_IS_WORKER){if(ENVIRONMENT_IS_WORKER){scriptDirectory=self.location.href}else if(typeof document!="undefined"&&document.currentScript){scriptDirectory=document.currentScript.src}if(scriptDirectory.startsWith("blob:")){scriptDirectory=""}else{scriptDirectory=scriptDirectory.substr(0,scriptDirectory.replace(/[?#].*/,"").lastIndexOf("/")+1)}{if(ENVIRONMENT_IS_WORKER){readBinary=url=>{var xhr=new XMLHttpRequest;xhr.open("GET",url,false);xhr.responseType="arraybuffer";xhr.send(null);return new Uint8Array(xhr.response)}}readAsync=async url=>{if(isFileURI(url)){return new Promise((resolve,reject)=>{var xhr=new XMLHttpRequest;xhr.open("GET",url,true);xhr.responseType="arraybuffer";xhr.onload=()=>{if(xhr.status==200||xhr.status==0&&xhr.response){resolve(xhr.response);return}reject(xhr.status)};xhr.onerror=reject;xhr.send(null)})}var response=await fetch(url,{credentials:"same-origin"});if(response.ok){return response.arrayBuffer()}throw new Error(response.status+" : "+response.url)}}}else{}var out=Module["print"]||console.log.bind(console);var err=Module["printErr"]||console.error.bind(console);Object.assign(Module,moduleOverrides);moduleOverrides=null;if(Module["arguments"])arguments_=Module["arguments"];if(Module["thisProgram"])thisProgram=Module["thisProgram"];var wasmBinary=Module["wasmBinary"];var wasmMemory;var ABORT=false;var EXITSTATUS;var HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF32,HEAP64,HEAPU64,HEAPF64;var runtimeInitialized=false;var dataURIPrefix="data:application/octet-stream;base64,";var isDataURI=filename=>filename.startsWith(dataURIPrefix);var isFileURI=filename=>filename.startsWith("file://");function updateMemoryViews(){var b=wasmMemory.buffer;Module["HEAP8"]=HEAP8=new Int8Array(b);Module["HEAP16"]=HEAP16=new Int16Array(b);Module["HEAPU8"]=HEAPU8=new Uint8Array(b);Module["HEAPU16"]=HEAPU16=new Uint16Array(b);Module["HEAP32"]=HEAP32=new Int32Array(b);Module["HEAPU32"]=HEAPU32=new Uint32Array(b);Module["HEAPF32"]=HEAPF32=new Float32Array(b);Module["HEAPF64"]=HEAPF64=new Float64Array(b);Module["HEAP64"]=HEAP64=new BigInt64Array(b);Module["HEAPU64"]=HEAPU64=new BigUint64Array(b)}var __ATPRERUN__=[];var __ATINIT__=[];var __ATMAIN__=[];var __ATPOSTRUN__=[];function preRun(){if(Module["preRun"]){if(typeof Module["preRun"]=="function")Module["preRun"]=[Module["preRun"]];while(Module["preRun"].length){addOnPreRun(Module["preRun"].shift())}}callRuntimeCallbacks(__ATPRERUN__)}function initRuntime(){runtimeInitialized=true;if(!Module["noFSInit"]&&!FS.initialized)FS.init();FS.ignorePermissions=false;TTY.init();callRuntimeCallbacks(__ATINIT__)}function preMain(){callRuntimeCallbacks(__ATMAIN__)}function postRun(){if(Module["postRun"]){if(typeof Module["postRun"]=="function")Module["postRun"]=[Module["postRun"]];while(Module["postRun"].length){addOnPostRun(Module["postRun"].shift())}}callRuntimeCallbacks(__ATPOSTRUN__)}function addOnPreRun(cb){__ATPRERUN__.unshift(cb)}function addOnInit(cb){__ATINIT__.unshift(cb)}function addOnPostRun(cb){__ATPOSTRUN__.unshift(cb)}var runDependencies=0;var dependenciesFulfilled=null;function getUniqueRunDependency(id){return id}function addRunDependency(id){runDependencies++;Module["monitorRunDependencies"]?.(runDependencies)}function removeRunDependency(id){runDependencies--;Module["monitorRunDependencies"]?.(runDependencies);if(runDependencies==0){if(dependenciesFulfilled){var callback=dependenciesFulfilled;dependenciesFulfilled=null;callback()}}}function abort(what){Module["onAbort"]?.(what);what="Aborted("+what+")";err(what);ABORT=true;what+=". Build with -sASSERTIONS for more info.";if(runtimeInitialized){___trap()}var e=new WebAssembly.RuntimeError(what);throw e}var wasmBinaryFile;function findWasmBinary(){var f="wllama.wasm";if(!isDataURI(f)){return locateFile(f)}return f}function getBinarySync(file){if(file==wasmBinaryFile&&wasmBinary){return new Uint8Array(wasmBinary)}if(readBinary){return readBinary(file)}throw"both async and sync fetching of the wasm failed"}async function getWasmBinary(binaryFile){if(!wasmBinary){try{var response=await readAsync(binaryFile);return new Uint8Array(response)}catch{}}return getBinarySync(binaryFile)}async function instantiateArrayBuffer(binaryFile,imports){try{var binary=await getWasmBinary(binaryFile);var instance=await WebAssembly.instantiate(binary,imports);return instance}catch(reason){err(`failed to asynchronously prepare wasm: ${reason}`);abort(reason)}}async function instantiateAsync(binary,binaryFile,imports){if(!binary&&typeof WebAssembly.instantiateStreaming=="function"&&!isDataURI(binaryFile)&&!isFileURI(binaryFile)&&!ENVIRONMENT_IS_NODE){try{var response=fetch(binaryFile,{credentials:"same-origin"});var instantiationResult=await WebAssembly.instantiateStreaming(response,imports);return instantiationResult}catch(reason){err(`wasm streaming compile failed: ${reason}`);err("falling back to ArrayBuffer instantiation")}}return instantiateArrayBuffer(binaryFile,imports)}function getWasmImports(){return{a:wasmImports}}async function createWasm(){function receiveInstance(instance,module){wasmExports=instance.exports;wasmExports=applySignatureConversions(wasmExports);wasmMemory=wasmExports["u"];updateMemoryViews();addOnInit(wasmExports["v"]);removeRunDependency("wasm-instantiate");return wasmExports}addRunDependency("wasm-instantiate");function receiveInstantiationResult(result){return receiveInstance(result["instance"])}var info=getWasmImports();if(Module["instantiateWasm"]){try{return Module["instantiateWasm"](info,receiveInstance)}catch(e){err(`Module.instantiateWasm callback failed with error: ${e}`);return false}}wasmBinaryFile??=findWasmBinary();var result=await instantiateAsync(wasmBinary,wasmBinaryFile,info);var exports=receiveInstantiationResult(result);return exports}class ExitStatus{name="ExitStatus";constructor(status){this.message=`Program terminated with exit(${status})`;this.status=status}}Module["ExitStatus"]=ExitStatus;var callRuntimeCallbacks=callbacks=>{while(callbacks.length>0){callbacks.shift()(Module)}};Module["callRuntimeCallbacks"]=callRuntimeCallbacks;function getValue(ptr,type="i8"){if(type.endsWith("*"))type="*";switch(type){case"i1":return HEAP8[ptr>>>0];case"i8":return HEAP8[ptr>>>0];case"i16":return HEAP16[ptr>>>1>>>0];case"i32":return HEAP32[ptr>>>2>>>0];case"i64":return HEAP64[ptr>>>3];case"float":return HEAPF32[ptr>>>2>>>0];case"double":return HEAPF64[ptr>>>3>>>0];case"*":return HEAPU32[ptr>>>2>>>0];default:abort(`invalid type for getValue: ${type}`)}}Module["getValue"]=getValue;var noExitRuntime=Module["noExitRuntime"]||true;Module["noExitRuntime"]=noExitRuntime;function setValue(ptr,value,type="i8"){if(type.endsWith("*"))type="*";switch(type){case"i1":HEAP8[ptr>>>0]=value;break;case"i8":HEAP8[ptr>>>0]=value;break;case"i16":HEAP16[ptr>>>1>>>0]=value;break;case"i32":HEAP32[ptr>>>2>>>0]=value;break;case"i64":HEAP64[ptr>>>3]=BigInt(value);break;case"float":HEAPF32[ptr>>>2>>>0]=value;break;case"double":HEAPF64[ptr>>>3>>>0]=value;break;case"*":HEAPU32[ptr>>>2>>>0]=value;break;default:abort(`invalid type for setValue: ${type}`)}}Module["setValue"]=setValue;var syscallGetVarargI=()=>{var ret=HEAP32[+SYSCALLS.varargs>>>2>>>0];SYSCALLS.varargs+=4;return ret};Module["syscallGetVarargI"]=syscallGetVarargI;var syscallGetVarargP=syscallGetVarargI;Module["syscallGetVarargP"]=syscallGetVarargP;var PATH={isAbs:path=>path.charAt(0)==="/",splitPath:filename=>{var splitPathRe=/^(\\/?|)([\\s\\S]*?)((?:\\.{1,2}|[^\\/]+?|)(\\.[^.\\/]*|))(?:[\\/]*)$/;return splitPathRe.exec(filename).slice(1)},normalizeArray:(parts,allowAboveRoot)=>{var up=0;for(var i=parts.length-1;i>=0;i--){var last=parts[i];if(last==="."){parts.splice(i,1)}else if(last===".."){parts.splice(i,1);up++}else if(up){parts.splice(i,1);up--}}if(allowAboveRoot){for(;up;up--){parts.unshift("..")}}return parts},normalize:path=>{var isAbsolute=PATH.isAbs(path),trailingSlash=path.substr(-1)==="/";path=PATH.normalizeArray(path.split("/").filter(p=>!!p),!isAbsolute).join("/");if(!path&&!isAbsolute){path="."}if(path&&trailingSlash){path+="/"}return(isAbsolute?"/":"")+path},dirname:path=>{var result=PATH.splitPath(path),root=result[0],dir=result[1];if(!root&&!dir){return"."}if(dir){dir=dir.substr(0,dir.length-1)}return root+dir},basename:path=>path&&path.match(/([^\\/]+|\\/)\\/*$/)[1],join:(...paths)=>PATH.normalize(paths.join("/")),join2:(l,r)=>PATH.normalize(l+"/"+r)};Module["PATH"]=PATH;var initRandomFill=()=>{if(ENVIRONMENT_IS_NODE){var nodeCrypto=require("crypto");return view=>nodeCrypto.randomFillSync(view)}return view=>crypto.getRandomValues(view)};Module["initRandomFill"]=initRandomFill;var randomFill=view=>{(randomFill=initRandomFill())(view)};Module["randomFill"]=randomFill;var PATH_FS={resolve:(...args)=>{var resolvedPath="",resolvedAbsolute=false;for(var i=args.length-1;i>=-1&&!resolvedAbsolute;i--){var path=i>=0?args[i]:FS.cwd();if(typeof path!="string"){throw new TypeError("Arguments to path.resolve must be strings")}else if(!path){return""}resolvedPath=path+"/"+resolvedPath;resolvedAbsolute=PATH.isAbs(path)}resolvedPath=PATH.normalizeArray(resolvedPath.split("/").filter(p=>!!p),!resolvedAbsolute).join("/");return(resolvedAbsolute?"/":"")+resolvedPath||"."},relative:(from,to)=>{from=PATH_FS.resolve(from).substr(1);to=PATH_FS.resolve(to).substr(1);function trim(arr){var start=0;for(;start<arr.length;start++){if(arr[start]!=="")break}var end=arr.length-1;for(;end>=0;end--){if(arr[end]!=="")break}if(start>end)return[];return arr.slice(start,end-start+1)}var fromParts=trim(from.split("/"));var toParts=trim(to.split("/"));var length=Math.min(fromParts.length,toParts.length);var samePartsLength=length;for(var i=0;i<length;i++){if(fromParts[i]!==toParts[i]){samePartsLength=i;break}}var outputParts=[];for(var i=samePartsLength;i<fromParts.length;i++){outputParts.push("..")}outputParts=outputParts.concat(toParts.slice(samePartsLength));return outputParts.join("/")}};Module["PATH_FS"]=PATH_FS;var UTF8Decoder=typeof TextDecoder!="undefined"?new TextDecoder:undefined;Module["UTF8Decoder"]=UTF8Decoder;var UTF8ArrayToString=(heapOrArray,idx=0,maxBytesToRead=NaN)=>{idx>>>=0;var endIdx=idx+maxBytesToRead;var endPtr=idx;while(heapOrArray[endPtr]&&!(endPtr>=endIdx))++endPtr;if(endPtr-idx>16&&heapOrArray.buffer&&UTF8Decoder){return UTF8Decoder.decode(heapOrArray.subarray(idx,endPtr))}var str="";while(idx<endPtr){var u0=heapOrArray[idx++];if(!(u0&128)){str+=String.fromCharCode(u0);continue}var u1=heapOrArray[idx++]&63;if((u0&224)==192){str+=String.fromCharCode((u0&31)<<6|u1);continue}var u2=heapOrArray[idx++]&63;if((u0&240)==224){u0=(u0&15)<<12|u1<<6|u2}else{u0=(u0&7)<<18|u1<<12|u2<<6|heapOrArray[idx++]&63}if(u0<65536){str+=String.fromCharCode(u0)}else{var ch=u0-65536;str+=String.fromCharCode(55296|ch>>10,56320|ch&1023)}}return str};Module["UTF8ArrayToString"]=UTF8ArrayToString;var FS_stdin_getChar_buffer=[];Module["FS_stdin_getChar_buffer"]=FS_stdin_getChar_buffer;var lengthBytesUTF8=str=>{var len=0;for(var i=0;i<str.length;++i){var c=str.charCodeAt(i);if(c<=127){len++}else if(c<=2047){len+=2}else if(c>=55296&&c<=57343){len+=4;++i}else{len+=3}}return len};Module["lengthBytesUTF8"]=lengthBytesUTF8;var stringToUTF8Array=(str,heap,outIdx,maxBytesToWrite)=>{outIdx>>>=0;if(!(maxBytesToWrite>0))return 0;var startIdx=outIdx;var endIdx=outIdx+maxBytesToWrite-1;for(var i=0;i<str.length;++i){var u=str.charCodeAt(i);if(u>=55296&&u<=57343){var u1=str.charCodeAt(++i);u=65536+((u&1023)<<10)|u1&1023}if(u<=127){if(outIdx>=endIdx)break;heap[outIdx++>>>0]=u}else if(u<=2047){if(outIdx+1>=endIdx)break;heap[outIdx++>>>0]=192|u>>6;heap[outIdx++>>>0]=128|u&63}else if(u<=65535){if(outIdx+2>=endIdx)break;heap[outIdx++>>>0]=224|u>>12;heap[outIdx++>>>0]=128|u>>6&63;heap[outIdx++>>>0]=128|u&63}else{if(outIdx+3>=endIdx)break;heap[outIdx++>>>0]=240|u>>18;heap[outIdx++>>>0]=128|u>>12&63;heap[outIdx++>>>0]=128|u>>6&63;heap[outIdx++>>>0]=128|u&63}}heap[outIdx>>>0]=0;return outIdx-startIdx};Module["stringToUTF8Array"]=stringToUTF8Array;function intArrayFromString(stringy,dontAddNull,length){var len=length>0?length:lengthBytesUTF8(stringy)+1;var u8array=new Array(len);var numBytesWritten=stringToUTF8Array(stringy,u8array,0,u8array.length);if(dontAddNull)u8array.length=numBytesWritten;return u8array}Module["intArrayFromString"]=intArrayFromString;var FS_stdin_getChar=()=>{if(!FS_stdin_getChar_buffer.length){var result=null;if(ENVIRONMENT_IS_NODE){var BUFSIZE=256;var buf=Buffer.alloc(BUFSIZE);var bytesRead=0;var fd=process.stdin.fd;try{bytesRead=fs.readSync(fd,buf,0,BUFSIZE)}catch(e){if(e.toString().includes("EOF"))bytesRead=0;else throw e}if(bytesRead>0){result=buf.slice(0,bytesRead).toString("utf-8")}}else if(typeof window!="undefined"&&typeof window.prompt=="function"){result=window.prompt("Input: ");if(result!==null){result+="\\n"}}else{}if(!result){return null}FS_stdin_getChar_buffer=intArrayFromString(result,true)}return FS_stdin_getChar_buffer.shift()};Module["FS_stdin_getChar"]=FS_stdin_getChar;var TTY={ttys:[],init(){},shutdown(){},register(dev,ops){TTY.ttys[dev]={input:[],output:[],ops};FS.registerDevice(dev,TTY.stream_ops)},stream_ops:{open(stream){var tty=TTY.ttys[stream.node.rdev];if(!tty){throw new FS.ErrnoError(43)}stream.tty=tty;stream.seekable=false},close(stream){stream.tty.ops.fsync(stream.tty)},fsync(stream){stream.tty.ops.fsync(stream.tty)},read(stream,buffer,offset,length,pos){if(!stream.tty||!stream.tty.ops.get_char){throw new FS.ErrnoError(60)}var bytesRead=0;for(var i=0;i<length;i++){var result;try{result=stream.tty.ops.get_char(stream.tty)}catch(e){throw new FS.ErrnoError(29)}if(result===undefined&&bytesRead===0){throw new FS.ErrnoError(6)}if(result===null||result===undefined)break;bytesRead++;buffer[offset+i]=result}if(bytesRead){stream.node.atime=Date.now()}return bytesRead},write(stream,buffer,offset,length,pos){if(!stream.tty||!stream.tty.ops.put_char){throw new FS.ErrnoError(60)}try{for(var i=0;i<length;i++){stream.tty.ops.put_char(stream.tty,buffer[offset+i])}}catch(e){throw new FS.ErrnoError(29)}if(length){stream.node.mtime=stream.node.ctime=Date.now()}return i}},default_tty_ops:{get_char(tty){return FS_stdin_getChar()},put_char(tty,val){if(val===null||val===10){out(UTF8ArrayToString(tty.output));tty.output=[]}else{if(val!=0)tty.output.push(val)}},fsync(tty){if(tty.output&&tty.output.length>0){out(UTF8ArrayToString(tty.output));tty.output=[]}},ioctl_tcgets(tty){return{c_iflag:25856,c_oflag:5,c_cflag:191,c_lflag:35387,c_cc:[3,28,127,21,4,0,1,0,17,19,26,0,18,15,23,22,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}},ioctl_tcsets(tty,optional_actions,data){return 0},ioctl_tiocgwinsz(tty){return[24,80]}},default_tty1_ops:{put_char(tty,val){if(val===null||val===10){err(UTF8ArrayToString(tty.output));tty.output=[]}else{if(val!=0)tty.output.push(val)}},fsync(tty){if(tty.output&&tty.output.length>0){err(UTF8ArrayToString(tty.output));tty.output=[]}}}};Module["TTY"]=TTY;var zeroMemory=(address,size)=>{HEAPU8.fill(0,address,address+size)};Module["zeroMemory"]=zeroMemory;var alignMemory=(size,alignment)=>Math.ceil(size/alignment)*alignment;Module["alignMemory"]=alignMemory;var mmapAlloc=size=>{size=alignMemory(size,65536);var ptr=_emscripten_builtin_memalign(65536,size);if(ptr)zeroMemory(ptr,size);return ptr};Module["mmapAlloc"]=mmapAlloc;var MEMFS={ops_table:null,mount(mount){return MEMFS.createNode(null,"/",16895,0)},createNode(parent,name,mode,dev){if(FS.isBlkdev(mode)||FS.isFIFO(mode)){throw new FS.ErrnoError(63)}MEMFS.ops_table||={dir:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr,lookup:MEMFS.node_ops.lookup,mknod:MEMFS.node_ops.mknod,rename:MEMFS.node_ops.rename,unlink:MEMFS.node_ops.unlink,rmdir:MEMFS.node_ops.rmdir,readdir:MEMFS.node_ops.readdir,symlink:MEMFS.node_ops.symlink},stream:{llseek:MEMFS.stream_ops.llseek}},file:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr},stream:{llseek:MEMFS.stream_ops.llseek,read:MEMFS.stream_ops.read,write:MEMFS.stream_ops.write,allocate:MEMFS.stream_ops.allocate,mmap:MEMFS.stream_ops.mmap,msync:MEMFS.stream_ops.msync}},link:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr,readlink:MEMFS.node_ops.readlink},stream:{}},chrdev:{node:{getattr:MEMFS.node_ops.getattr,setattr:MEMFS.node_ops.setattr},stream:FS.chrdev_stream_ops}};var node=FS.createNode(parent,name,mode,dev);if(FS.isDir(node.mode)){node.node_ops=MEMFS.ops_table.dir.node;node.stream_ops=MEMFS.ops_table.dir.stream;node.contents={}}else if(FS.isFile(node.mode)){node.node_ops=MEMFS.ops_table.file.node;node.stream_ops=MEMFS.ops_table.file.stream;node.usedBytes=0;node.contents=null}else if(FS.isLink(node.mode)){node.node_ops=MEMFS.ops_table.link.node;node.stream_ops=MEMFS.ops_table.link.stream}else if(FS.isChrdev(node.mode)){node.node_ops=MEMFS.ops_table.chrdev.node;node.stream_ops=MEMFS.ops_table.chrdev.stream}node.atime=node.mtime=node.ctime=Date.now();if(parent){parent.contents[name]=node;parent.atime=parent.mtime=parent.ctime=node.atime}return node},getFileDataAsTypedArray(node){if(!node.contents)return new Uint8Array(0);if(node.contents.subarray)return node.contents.subarray(0,node.usedBytes);return new Uint8Array(node.contents)},expandFileStorage(node,newCapacity){var prevCapacity=node.contents?node.contents.length:0;if(prevCapacity>=newCapacity)return;var CAPACITY_DOUBLING_MAX=1024*1024;newCapacity=Math.max(newCapacity,prevCapacity*(prevCapacity<CAPACITY_DOUBLING_MAX?2:1.125)>>>0);if(prevCapacity!=0)newCapacity=Math.max(newCapacity,256);var oldContents=node.contents;node.contents=new Uint8Array(newCapacity);if(node.usedBytes>0)node.contents.set(oldContents.subarray(0,node.usedBytes),0)},resizeFileStorage(node,newSize){if(node.usedBytes==newSize)return;if(newSize==0){node.contents=null;node.usedBytes=0}else{var oldContents=node.contents;node.contents=new Uint8Array(newSize);if(oldContents){node.contents.set(oldContents.subarray(0,Math.min(newSize,node.usedBytes)))}node.usedBytes=newSize}},node_ops:{getattr(node){var attr={};attr.dev=FS.isChrdev(node.mode)?node.id:1;attr.ino=node.id;attr.mode=node.mode;attr.nlink=1;attr.uid=0;attr.gid=0;attr.rdev=node.rdev;if(FS.isDir(node.mode)){attr.size=4096}else if(FS.isFile(node.mode)){attr.size=node.usedBytes}else if(FS.isLink(node.mode)){attr.size=node.link.length}else{attr.size=0}attr.atime=new Date(node.atime);attr.mtime=new Date(node.mtime);attr.ctime=new Date(node.ctime);attr.blksize=4096;attr.blocks=Math.ceil(attr.size/attr.blksize);return attr},setattr(node,attr){for(const key of["mode","atime","mtime","ctime"]){if(attr[key]!=null){node[key]=attr[key]}}if(attr.size!==undefined){MEMFS.resizeFileStorage(node,attr.size)}},lookup(parent,name){throw MEMFS.doesNotExistError},mknod(parent,name,mode,dev){return MEMFS.createNode(parent,name,mode,dev)},rename(old_node,new_dir,new_name){var new_node;try{new_node=FS.lookupNode(new_dir,new_name)}catch(e){}if(new_node){if(FS.isDir(old_node.mode)){for(var i in new_node.contents){throw new FS.ErrnoError(55)}}FS.hashRemoveNode(new_node)}delete old_node.parent.contents[old_node.name];new_dir.contents[new_name]=old_node;old_node.name=new_name;new_dir.ctime=new_dir.mtime=old_node.parent.ctime=old_node.parent.mtime=Date.now()},unlink(parent,name){delete parent.contents[name];parent.ctime=parent.mtime=Date.now()},rmdir(parent,name){var node=FS.lookupNode(parent,name);for(var i in node.contents){throw new FS.ErrnoError(55)}delete parent.contents[name];parent.ctime=parent.mtime=Date.now()},readdir(node){return[".","..",...Object.keys(node.contents)]},symlink(parent,newname,oldpath){var node=MEMFS.createNode(parent,newname,511|40960,0);node.link=oldpath;return node},readlink(node){if(!FS.isLink(node.mode)){throw new FS.ErrnoError(28)}return node.link}},stream_ops:{read(stream,buffer,offset,length,position){var contents=stream.node.contents;if(position>=stream.node.usedBytes)return 0;var size=Math.min(stream.node.usedBytes-position,length);if(size>8&&contents.subarray){buffer.set(contents.subarray(position,position+size),offset)}else{for(var i=0;i<size;i++)buffer[offset+i]=contents[position+i]}return size},write(stream,buffer,offset,length,position,canOwn){if(buffer.buffer===HEAP8.buffer){canOwn=false}if(!length)return 0;var node=stream.node;node.mtime=node.ctime=Date.now();if(buffer.subarray&&(!node.contents||node.contents.subarray)){if(canOwn){node.contents=buffer.subarray(offset,offset+length);node.usedBytes=length;return length}else if(node.usedBytes===0&&position===0){node.contents=buffer.slice(offset,offset+length);node.usedBytes=length;return length}else if(position+length<=node.usedBytes){node.contents.set(buffer.subarray(offset,offset+length),position);return length}}MEMFS.expandFileStorage(node,position+length);if(node.contents.subarray&&buffer.subarray){node.contents.set(buffer.subarray(offset,offset+length),position)}else{for(var i=0;i<length;i++){node.contents[position+i]=buffer[offset+i]}}node.usedBytes=Math.max(node.usedBytes,position+length);return length},llseek(stream,offset,whence){var position=offset;if(whence===1){position+=stream.position}else if(whence===2){if(FS.isFile(stream.node.mode)){position+=stream.node.usedBytes}}if(position<0){throw new FS.ErrnoError(28)}return position},allocate(stream,offset,length){MEMFS.expandFileStorage(stream.node,offset+length);stream.node.usedBytes=Math.max(stream.node.usedBytes,offset+length)},mmap(stream,length,position,prot,flags){if(!FS.isFile(stream.node.mode)){throw new FS.ErrnoError(43)}var ptr;var allocated;var contents=stream.node.contents;if(!(flags&2)&&contents&&contents.buffer===HEAP8.buffer){allocated=false;ptr=contents.byteOffset}else{allocated=true;ptr=mmapAlloc(length);if(!ptr){throw new FS.ErrnoError(48)}if(contents){if(position>0||position+length<contents.length){if(contents.subarray){contents=contents.subarray(position,position+length)}else{contents=Array.prototype.slice.call(contents,position,position+length)}}HEAP8.set(contents,ptr>>>0)}}return{ptr,allocated}},msync(stream,buffer,offset,length,mmapFlags){MEMFS.stream_ops.write(stream,buffer,0,length,offset,false);return 0}}};Module["MEMFS"]=MEMFS;var asyncLoad=async url=>{var arrayBuffer=await readAsync(url);return new Uint8Array(arrayBuffer)};Module["asyncLoad"]=asyncLoad;var FS_createDataFile=(parent,name,fileData,canRead,canWrite,canOwn)=>{FS.createDataFile(parent,name,fileData,canRead,canWrite,canOwn)};Module["FS_createDataFile"]=FS_createDataFile;var preloadPlugins=Module["preloadPlugins"]||[];Module["preloadPlugins"]=preloadPlugins;var FS_handledByPreloadPlugin=(byteArray,fullname,finish,onerror)=>{if(typeof Browser!="undefined")Browser.init();var handled=false;preloadPlugins.forEach(plugin=>{if(handled)return;if(plugin["canHandle"](fullname)){plugin["handle"](byteArray,fullname,finish,onerror);handled=true}});return handled};Module["FS_handledByPreloadPlugin"]=FS_handledByPreloadPlugin;var FS_createPreloadedFile=(parent,name,url,canRead,canWrite,onload,onerror,dontCreateFile,canOwn,preFinish)=>{var fullname=name?PATH_FS.resolve(PATH.join2(parent,name)):parent;var dep=getUniqueRunDependency(`cp ${fullname}`);function processData(byteArray){function finish(byteArray){preFinish?.();if(!dontCreateFile){FS_createDataFile(parent,name,byteArray,canRead,canWrite,canOwn)}onload?.();removeRunDependency(dep)}if(FS_handledByPreloadPlugin(byteArray,fullname,finish,()=>{onerror?.();removeRunDependency(dep)})){return}finish(byteArray)}addRunDependency(dep);if(typeof url=="string"){asyncLoad(url).then(processData,onerror)}else{processData(url)}};Module["FS_createPreloadedFile"]=FS_createPreloadedFile;var FS_modeStringToFlags=str=>{var flagModes={r:0,"r+":2,w:512|64|1,"w+":512|64|2,a:1024|64|1,"a+":1024|64|2};var flags=flagModes[str];if(typeof flags=="undefined"){throw new Error(`Unknown file open mode: ${str}`)}return flags};Module["FS_modeStringToFlags"]=FS_modeStringToFlags;var FS_getMode=(canRead,canWrite)=>{var mode=0;if(canRead)mode|=292|73;if(canWrite)mode|=146;return mode};Module["FS_getMode"]=FS_getMode;var FS={root:null,mounts:[],devices:{},streams:[],nextInode:1,nameTable:null,currentPath:"/",initialized:false,ignorePermissions:true,ErrnoError:class{name="ErrnoError";constructor(errno){this.errno=errno}},filesystems:null,syncFSRequests:0,readFiles:{},FSStream:class{shared={};get object(){return this.node}set object(val){this.node=val}get isRead(){return(this.flags&2097155)!==1}get isWrite(){return(this.flags&2097155)!==0}get isAppend(){return this.flags&1024}get flags(){return this.shared.flags}set flags(val){this.shared.flags=val}get position(){return this.shared.position}set position(val){this.shared.position=val}},FSNode:class{node_ops={};stream_ops={};readMode=292|73;writeMode=146;mounted=null;constructor(parent,name,mode,rdev){if(!parent){parent=this}this.parent=parent;this.mount=parent.mount;this.id=FS.nextInode++;this.name=name;this.mode=mode;this.rdev=rdev;this.atime=this.mtime=this.ctime=Date.now()}get read(){return(this.mode&this.readMode)===this.readMode}set read(val){val?this.mode|=this.readMode:this.mode&=~this.readMode}get write(){return(this.mode&this.writeMode)===this.writeMode}set write(val){val?this.mode|=this.writeMode:this.mode&=~this.writeMode}get isFolder(){return FS.isDir(this.mode)}get isDevice(){return FS.isChrdev(this.mode)}},lookupPath(path,opts={}){if(!path){throw new FS.ErrnoError(44)}opts.follow_mount??=true;if(!PATH.isAbs(path)){path=FS.cwd()+"/"+path}linkloop:for(var nlinks=0;nlinks<40;nlinks++){var parts=path.split("/").filter(p=>!!p);var current=FS.root;var current_path="/";for(var i=0;i<parts.length;i++){var islast=i===parts.length-1;if(islast&&opts.parent){break}if(parts[i]==="."){continue}if(parts[i]===".."){current_path=PATH.dirname(current_path);current=current.parent;continue}current_path=PATH.join2(current_path,parts[i]);try{current=FS.lookupNode(current,parts[i])}catch(e){if(e?.errno===44&&islast&&opts.noent_okay){return{path:current_path}}throw e}if(FS.isMountpoint(current)&&(!islast||opts.follow_mount)){current=current.mounted.root}if(FS.isLink(current.mode)&&(!islast||opts.follow)){if(!current.node_ops.readlink){throw new FS.ErrnoError(52)}var link=current.node_ops.readlink(current);if(!PATH.isAbs(link)){link=PATH.dirname(current_path)+"/"+link}path=link+"/"+parts.slice(i+1).join("/");continue linkloop}}return{path:current_path,node:current}}throw new FS.ErrnoError(32)},getPath(node){var path;while(true){if(FS.isRoot(node)){var mount=node.mount.mountpoint;if(!path)return mount;return mount[mount.length-1]!=="/"?`${mount}/${path}`:mount+path}path=path?`${node.name}/${path}`:node.name;node=node.parent}},hashName(parentid,name){var hash=0;for(var i=0;i<name.length;i++){hash=(hash<<5)-hash+name.charCodeAt(i)|0}return(parentid+hash>>>0)%FS.nameTable.length},hashAddNode(node){var hash=FS.hashName(node.parent.id,node.name);node.name_next=FS.nameTable[hash];FS.nameTable[hash]=node},hashRemoveNode(node){var hash=FS.hashName(node.parent.id,node.name);if(FS.nameTable[hash]===node){FS.nameTable[hash]=node.name_next}else{var current=FS.nameTable[hash];while(current){if(current.name_next===node){current.name_next=node.name_next;break}current=current.name_next}}},lookupNode(parent,name){var errCode=FS.mayLookup(parent);if(errCode){throw new FS.ErrnoError(errCode)}var hash=FS.hashName(parent.id,name);for(var node=FS.nameTable[hash];node;node=node.name_next){var nodeName=node.name;if(node.parent.id===parent.id&&nodeName===name){return node}}return FS.lookup(parent,name)},createNode(parent,name,mode,rdev){var node=new FS.FSNode(parent,name,mode,rdev);FS.hashAddNode(node);return node},destroyNode(node){FS.hashRemoveNode(node)},isRoot(node){return node===node.parent},isMountpoint(node){return!!node.mounted},isFile(mode){return(mode&61440)===32768},isDir(mode){return(mode&61440)===16384},isLink(mode){return(mode&61440)===40960},isChrdev(mode){return(mode&61440)===8192},isBlkdev(mode){return(mode&61440)===24576},isFIFO(mode){return(mode&61440)===4096},isSocket(mode){return(mode&49152)===49152},flagsToPermissionString(flag){var perms=["r","w","rw"][flag&3];if(flag&512){perms+="w"}return perms},nodePermissions(node,perms){if(FS.ignorePermissions){return 0}if(perms.includes("r")&&!(node.mode&292)){return 2}else if(perms.includes("w")&&!(node.mode&146)){return 2}else if(perms.includes("x")&&!(node.mode&73)){return 2}return 0},mayLookup(dir){if(!FS.isDir(dir.mode))return 54;var errCode=FS.nodePermissions(dir,"x");if(errCode)return errCode;if(!dir.node_ops.lookup)return 2;return 0},mayCreate(dir,name){if(!FS.isDir(dir.mode)){return 54}try{var node=FS.lookupNode(dir,name);return 20}catch(e){}return FS.nodePermissions(dir,"wx")},mayDelete(dir,name,isdir){var node;try{node=FS.lookupNode(dir,name)}catch(e){return e.errno}var errCode=FS.nodePermissions(dir,"wx");if(errCode){return errCode}if(isdir){if(!FS.isDir(node.mode)){return 54}if(FS.isRoot(node)||FS.getPath(node)===FS.cwd()){return 10}}else{if(FS.isDir(node.mode)){return 31}}return 0},mayOpen(node,flags){if(!node){return 44}if(FS.isLink(node.mode)){return 32}else if(FS.isDir(node.mode)){if(FS.flagsToPermissionString(flags)!=="r"||flags&(512|64)){return 31}}return FS.nodePermissions(node,FS.flagsToPermissionString(flags))},checkOpExists(op,err){if(!op){throw new FS.ErrnoError(err)}return op},MAX_OPEN_FDS:4096,nextfd(){for(var fd=0;fd<=FS.MAX_OPEN_FDS;fd++){if(!FS.streams[fd]){return fd}}throw new FS.ErrnoError(33)},getStreamChecked(fd){var stream=FS.getStream(fd);if(!stream){throw new FS.ErrnoError(8)}return stream},getStream:fd=>FS.streams[fd],createStream(stream,fd=-1){stream=Object.assign(new FS.FSStream,stream);if(fd==-1){fd=FS.nextfd()}stream.fd=fd;FS.streams[fd]=stream;return stream},closeStream(fd){FS.streams[fd]=null},dupStream(origStream,fd=-1){var stream=FS.createStream(origStream,fd);stream.stream_ops?.dup?.(stream);return stream},chrdev_stream_ops:{open(stream){var device=FS.getDevice(stream.node.rdev);stream.stream_ops=device.stream_ops;stream.stream_ops.open?.(stream)},llseek(){throw new FS.ErrnoError(70)}},major:dev=>dev>>8,minor:dev=>dev&255,makedev:(ma,mi)=>ma<<8|mi,registerDevice(dev,ops){FS.devices[dev]={stream_ops:ops}},getDevice:dev=>FS.devices[dev],getMounts(mount){var mounts=[];var check=[mount];while(check.length){var m=check.pop();mounts.push(m);check.push(...m.mounts)}return mounts},syncfs(populate,callback){if(typeof populate=="function"){callback=populate;populate=false}FS.syncFSRequests++;if(FS.syncFSRequests>1){err(`warning: ${FS.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`)}var mounts=FS.getMounts(FS.root.mount);var completed=0;function doCallback(errCode){FS.syncFSRequests--;return callback(errCode)}function done(errCode){if(errCode){if(!done.errored){done.errored=true;return doCallback(errCode)}return}if(++completed>=mounts.length){doCallback(null)}}mounts.forEach(mount=>{if(!mount.type.syncfs){return done(null)}mount.type.syncfs(mount,populate,done)})},mount(type,opts,mountpoint){var root=mountpoint==="/";var pseudo=!mountpoint;var node;if(root&&FS.root){throw new FS.ErrnoError(10)}else if(!root&&!pseudo){var lookup=FS.lookupPath(mountpoint,{follow_mount:false});mountpoint=lookup.path;node=lookup.node;if(FS.isMountpoint(node)){throw new FS.ErrnoError(10)}if(!FS.isDir(node.mode)){throw new FS.ErrnoError(54)}}var mount={type,opts,mountpoint,mounts:[]};var mountRoot=type.mount(mount);mountRoot.mount=mount;mount.root=mountRoot;if(root){FS.root=mountRoot}else if(node){node.mounted=mount;if(node.mount){node.mount.mounts.push(mount)}}return mountRoot},unmount(mountpoint){var lookup=FS.lookupPath(mountpoint,{follow_mount:false});if(!FS.isMountpoint(lookup.node)){throw new FS.ErrnoError(28)}var node=lookup.node;var mount=node.mounted;var mounts=FS.getMounts(mount);Object.keys(FS.nameTable).forEach(hash=>{var current=FS.nameTable[hash];while(current){var next=current.name_next;if(mounts.includes(current.mount)){FS.destroyNode(current)}current=next}});node.mounted=null;var idx=node.mount.mounts.indexOf(mount);node.mount.mounts.splice(idx,1)},lookup(parent,name){return parent.node_ops.lookup(parent,name)},mknod(path,mode,dev){var lookup=FS.lookupPath(path,{parent:true});var parent=lookup.node;var name=PATH.basename(path);if(!name){throw new FS.ErrnoError(28)}if(name==="."||name===".."){throw new FS.ErrnoError(20)}var errCode=FS.mayCreate(parent,name);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.mknod){throw new FS.ErrnoError(63)}return parent.node_ops.mknod(parent,name,mode,dev)},statfs(path){var rtn={bsize:4096,frsize:4096,blocks:1e6,bfree:5e5,bavail:5e5,files:FS.nextInode,ffree:FS.nextInode-1,fsid:42,flags:2,namelen:255};var parent=FS.lookupPath(path,{follow:true}).node;if(parent?.node_ops.statfs){Object.assign(rtn,parent.node_ops.statfs(parent.mount.opts.root))}return rtn},create(path,mode=438){mode&=4095;mode|=32768;return FS.mknod(path,mode,0)},mkdir(path,mode=511){mode&=511|512;mode|=16384;return FS.mknod(path,mode,0)},mkdirTree(path,mode){var dirs=path.split("/");var d="";for(var i=0;i<dirs.length;++i){if(!dirs[i])continue;d+="/"+dirs[i];try{FS.mkdir(d,mode)}catch(e){if(e.errno!=20)throw e}}},mkdev(path,mode,dev){if(typeof dev=="undefined"){dev=mode;mode=438}mode|=8192;return FS.mknod(path,mode,dev)},symlink(oldpath,newpath){if(!PATH_FS.resolve(oldpath)){throw new FS.ErrnoError(44)}var lookup=FS.lookupPath(newpath,{parent:true});var parent=lookup.node;if(!parent){throw new FS.ErrnoError(44)}var newname=PATH.basename(newpath);var errCode=FS.mayCreate(parent,newname);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.symlink){throw new FS.ErrnoError(63)}return parent.node_ops.symlink(parent,newname,oldpath)},rename(old_path,new_path){var old_dirname=PATH.dirname(old_path);var new_dirname=PATH.dirname(new_path);var old_name=PATH.basename(old_path);var new_name=PATH.basename(new_path);var lookup,old_dir,new_dir;lookup=FS.lookupPath(old_path,{parent:true});old_dir=lookup.node;lookup=FS.lookupPath(new_path,{parent:true});new_dir=lookup.node;if(!old_dir||!new_dir)throw new FS.ErrnoError(44);if(old_dir.mount!==new_dir.mount){throw new FS.ErrnoError(75)}var old_node=FS.lookupNode(old_dir,old_name);var relative=PATH_FS.relative(old_path,new_dirname);if(relative.charAt(0)!=="."){throw new FS.ErrnoError(28)}relative=PATH_FS.relative(new_path,old_dirname);if(relative.charAt(0)!=="."){throw new FS.ErrnoError(55)}var new_node;try{new_node=FS.lookupNode(new_dir,new_name)}catch(e){}if(old_node===new_node){return}var isdir=FS.isDir(old_node.mode);var errCode=FS.mayDelete(old_dir,old_name,isdir);if(errCode){throw new FS.ErrnoError(errCode)}errCode=new_node?FS.mayDelete(new_dir,new_name,isdir):FS.mayCreate(new_dir,new_name);if(errCode){throw new FS.ErrnoError(errCode)}if(!old_dir.node_ops.rename){throw new FS.ErrnoError(63)}if(FS.isMountpoint(old_node)||new_node&&FS.isMountpoint(new_node)){throw new FS.ErrnoError(10)}if(new_dir!==old_dir){errCode=FS.nodePermissions(old_dir,"w");if(errCode){throw new FS.ErrnoError(errCode)}}FS.hashRemoveNode(old_node);try{old_dir.node_ops.rename(old_node,new_dir,new_name);old_node.parent=new_dir}catch(e){throw e}finally{FS.hashAddNode(old_node)}},rmdir(path){var lookup=FS.lookupPath(path,{parent:true});var parent=lookup.node;var name=PATH.basename(path);var node=FS.lookupNode(parent,name);var errCode=FS.mayDelete(parent,name,true);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.rmdir){throw new FS.ErrnoError(63)}if(FS.isMountpoint(node)){throw new FS.ErrnoError(10)}parent.node_ops.rmdir(parent,name);FS.destroyNode(node)},readdir(path){var lookup=FS.lookupPath(path,{follow:true});var node=lookup.node;var readdir=FS.checkOpExists(node.node_ops.readdir,54);return readdir(node)},unlink(path){var lookup=FS.lookupPath(path,{parent:true});var parent=lookup.node;if(!parent){throw new FS.ErrnoError(44)}var name=PATH.basename(path);var node=FS.lookupNode(parent,name);var errCode=FS.mayDelete(parent,name,false);if(errCode){throw new FS.ErrnoError(errCode)}if(!parent.node_ops.unlink){throw new FS.ErrnoError(63)}if(FS.isMountpoint(node)){throw new FS.ErrnoError(10)}parent.node_ops.unlink(parent,name);FS.destroyNode(node)},readlink(path){var lookup=FS.lookupPath(path);var link=lookup.node;if(!link){throw new FS.ErrnoError(44)}if(!link.node_ops.readlink){throw new FS.ErrnoError(28)}return link.node_ops.readlink(link)},stat(path,dontFollow){var lookup=FS.lookupPath(path,{follow:!dontFollow});var node=lookup.node;var getattr=FS.checkOpExists(node.node_ops.getattr,63);return getattr(node)},lstat(path){return FS.stat(path,true)},chmod(path,mode,dontFollow){var node;if(typeof path=="string"){var lookup=FS.lookupPath(path,{follow:!dontFollow});node=lookup.node}else{node=path}var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{mode:mode&4095|node.mode&~4095,ctime:Date.now(),dontFollow})},lchmod(path,mode){FS.chmod(path,mode,true)},fchmod(fd,mode){var stream=FS.getStreamChecked(fd);FS.chmod(stream.node,mode)},chown(path,uid,gid,dontFollow){var node;if(typeof path=="string"){var lookup=FS.lookupPath(path,{follow:!dontFollow});node=lookup.node}else{node=path}var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{timestamp:Date.now(),dontFollow})},lchown(path,uid,gid){FS.chown(path,uid,gid,true)},fchown(fd,uid,gid){var stream=FS.getStreamChecked(fd);FS.chown(stream.node,uid,gid)},truncate(path,len){if(len<0){throw new FS.ErrnoError(28)}var node;if(typeof path=="string"){var lookup=FS.lookupPath(path,{follow:true});node=lookup.node}else{node=path}if(FS.isDir(node.mode)){throw new FS.ErrnoError(31)}if(!FS.isFile(node.mode)){throw new FS.ErrnoError(28)}var errCode=FS.nodePermissions(node,"w");if(errCode){throw new FS.ErrnoError(errCode)}var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{size:len,timestamp:Date.now()})},ftruncate(fd,len){var stream=FS.getStreamChecked(fd);if((stream.flags&2097155)===0){throw new FS.ErrnoError(28)}FS.truncate(stream.node,len)},utime(path,atime,mtime){var lookup=FS.lookupPath(path,{follow:true});var node=lookup.node;var setattr=FS.checkOpExists(node.node_ops.setattr,63);setattr(node,{atime,mtime})},open(path,flags,mode=438){if(path===""){throw new FS.ErrnoError(44)}flags=typeof flags=="string"?FS_modeStringToFlags(flags):flags;if(flags&64){mode=mode&4095|32768}else{mode=0}var node;var isDirPath;if(typeof path=="object"){node=path}else{isDirPath=path.endsWith("/");var lookup=FS.lookupPath(path,{follow:!(flags&131072),noent_okay:true});node=lookup.node;path=lookup.path}var created=false;if(flags&64){if(node){if(flags&128){throw new FS.ErrnoError(20)}}else if(isDirPath){throw new FS.ErrnoError(31)}else{node=FS.mknod(path,mode|511,0);created=true}}if(!node){throw new FS.ErrnoError(44)}if(FS.isChrdev(node.mode)){flags&=~512}if(flags&65536&&!FS.isDir(node.mode)){throw new FS.ErrnoError(54)}if(!created){var errCode=FS.mayOpen(node,flags);if(errCode){throw new FS.ErrnoError(errCode)}}if(flags&512&&!created){FS.truncate(node,0)}flags&=~(128|512|131072);var stream=FS.createStream({node,path:FS.getPath(node),flags,seekable:true,position:0,stream_ops:node.stream_ops,ungotten:[],error:false});if(stream.stream_ops.open){stream.stream_ops.open(stream)}if(created){FS.chmod(node,mode&511)}if(Module["logReadFiles"]&&!(flags&1)){if(!(path in FS.readFiles)){FS.readFiles[path]=1}}return stream},close(stream){if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if(stream.getdents)stream.getdents=null;try{if(stream.stream_ops.close){stream.stream_ops.close(stream)}}catch(e){throw e}finally{FS.closeStream(stream.fd)}stream.fd=null},isClosed(stream){return stream.fd===null},llseek(stream,offset,whence){if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if(!stream.seekable||!stream.stream_ops.llseek){throw new FS.ErrnoError(70)}if(whence!=0&&whence!=1&&whence!=2){throw new FS.ErrnoError(28)}stream.position=stream.stream_ops.llseek(stream,offset,whence);stream.ungotten=[];return stream.position},read(stream,buffer,offset,length,position){if(length<0||position<0){throw new FS.ErrnoError(28)}if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if((stream.flags&2097155)===1){throw new FS.ErrnoError(8)}if(FS.isDir(stream.node.mode)){throw new FS.ErrnoError(31)}if(!stream.stream_ops.read){throw new FS.ErrnoError(28)}var seeking=typeof position!="undefined";if(!seeking){position=stream.position}else if(!stream.seekable){throw new FS.ErrnoError(70)}var bytesRead=stream.stream_ops.read(stream,buffer,offset,length,position);if(!seeking)stream.position+=bytesRead;return bytesRead},write(stream,buffer,offset,length,position,canOwn){if(length<0||position<0){throw new FS.ErrnoError(28)}if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if((stream.flags&2097155)===0){throw new FS.ErrnoError(8)}if(FS.isDir(stream.node.mode)){throw new FS.ErrnoError(31)}if(!stream.stream_ops.write){throw new FS.ErrnoError(28)}if(stream.seekable&&stream.flags&1024){FS.llseek(stream,0,2)}var seeking=typeof position!="undefined";if(!seeking){position=stream.position}else if(!stream.seekable){throw new FS.ErrnoError(70)}var bytesWritten=stream.stream_ops.write(stream,buffer,offset,length,position,canOwn);if(!seeking)stream.position+=bytesWritten;return bytesWritten},allocate(stream,offset,length){if(FS.isClosed(stream)){throw new FS.ErrnoError(8)}if(offset<0||length<=0){throw new FS.ErrnoError(28)}if((stream.flags&2097155)===0){throw new FS.ErrnoError(8)}if(!FS.isFile(stream.node.mode)&&!FS.isDir(stream.node.mode)){throw new FS.ErrnoError(43)}if(!stream.stream_ops.allocate){throw new FS.ErrnoError(138)}stream.stream_ops.allocate(stream,offset,length)},mmap(stream,length,position,prot,flags){if((prot&2)!==0&&(flags&2)===0&&(stream.flags&2097155)!==2){throw new FS.ErrnoError(2)}if((stream.flags&2097155)===1){throw new FS.ErrnoError(2)}if(!stream.stream_ops.mmap){throw new FS.ErrnoError(43)}if(!length){throw new FS.ErrnoError(28)}return stream.stream_ops.mmap(stream,length,position,prot,flags)},msync(stream,buffer,offset,length,mmapFlags){if(!stream.stream_ops.msync){return 0}return stream.stream_ops.msync(stream,buffer,offset,length,mmapFlags)},ioctl(stream,cmd,arg){if(!stream.stream_ops.ioctl){throw new FS.ErrnoError(59)}return stream.stream_ops.ioctl(stream,cmd,arg)},readFile(path,opts={}){opts.flags=opts.flags||0;opts.encoding=opts.encoding||"binary";if(opts.encoding!=="utf8"&&opts.encoding!=="binary"){throw new Error(`Invalid encoding type "${opts.encoding}"`)}var ret;var stream=FS.open(path,opts.flags);var stat=FS.stat(path);var length=stat.size;var buf=new Uint8Array(length);FS.read(stream,buf,0,length,0);if(opts.encoding==="utf8"){ret=UTF8ArrayToString(buf)}else if(opts.encoding==="binary"){ret=buf}FS.close(stream);return ret},writeFile(path,data,opts={}){opts.flags=opts.flags||577;var stream=FS.open(path,opts.flags,opts.mode);if(typeof data=="string"){var buf=new Uint8Array(lengthBytesUTF8(data)+1);var actualNumBytes=stringToUTF8Array(data,buf,0,buf.length);FS.write(stream,buf,0,actualNumBytes,undefined,opts.canOwn)}else if(ArrayBuffer.isView(data)){FS.write(stream,data,0,data.byteLength,undefined,opts.canOwn)}else{throw new Error("Unsupported data type")}FS.close(stream)},cwd:()=>FS.currentPath,chdir(path){var lookup=FS.lookupPath(path,{follow:true});if(lookup.node===null){throw new FS.ErrnoError(44)}if(!FS.isDir(lookup.node.mode)){throw new FS.ErrnoError(54)}var errCode=FS.nodePermissions(lookup.node,"x");if(errCode){throw new FS.ErrnoError(errCode)}FS.currentPath=lookup.path},createDefaultDirectories(){FS.mkdir("/tmp");FS.mkdir("/home");FS.mkdir("/home/web_user")},createDefaultDevices(){FS.mkdir("/dev");FS.registerDevice(FS.makedev(1,3),{read:()=>0,write:(stream,buffer,offset,length,pos)=>length,llseek:()=>0});FS.mkdev("/dev/null",FS.makedev(1,3));TTY.register(FS.makedev(5,0),TTY.default_tty_ops);TTY.register(FS.makedev(6,0),TTY.default_tty1_ops);FS.mkdev("/dev/tty",FS.makedev(5,0));FS.mkdev("/dev/tty1",FS.makedev(6,0));var randomBuffer=new Uint8Array(1024),randomLeft=0;var randomByte=()=>{if(randomLeft===0){randomFill(randomBuffer);randomLeft=randomBuffer.byteLength}return randomBuffer[--randomLeft]};FS.createDevice("/dev","random",randomByte);FS.createDevice("/dev","urandom",randomByte);FS.mkdir("/dev/shm");FS.mkdir("/dev/shm/tmp")},createSpecialDirectories(){FS.mkdir("/proc");var proc_self=FS.mkdir("/proc/self");FS.mkdir("/proc/self/fd");FS.mount({mount(){var node=FS.createNode(proc_self,"fd",16895,73);node.stream_ops={llseek:MEMFS.stream_ops.llseek};node.node_ops={lookup(parent,name){var fd=+name;var stream=FS.getStreamChecked(fd);var ret={parent:null,mount:{mountpoint:"fake"},node_ops:{readlink:()=>stream.path},id:fd+1};ret.parent=ret;return ret},readdir(){return Array.from(FS.streams.entries()).filter(([k,v])=>v).map(([k,v])=>k.toString())}};return node}},{},"/proc/self/fd")},createStandardStreams(input,output,error){if(input){FS.createDevice("/dev","stdin",input)}else{FS.symlink("/dev/tty","/dev/stdin")}if(output){FS.createDevice("/dev","stdout",null,output)}else{FS.symlink("/dev/tty","/dev/stdout")}if(error){FS.createDevice("/dev","stderr",null,error)}else{FS.symlink("/dev/tty1","/dev/stderr")}var stdin=FS.open("/dev/stdin",0);var stdout=FS.open("/dev/stdout",1);var stderr=FS.open("/dev/stderr",1)},staticInit(){FS.nameTable=new Array(4096);FS.mount(MEMFS,{},"/");FS.createDefaultDirectories();FS.createDefaultDevices();FS.createSpecialDirectories();FS.filesystems={MEMFS}},init(input,output,error){FS.initialized=true;input??=Module["stdin"];output??=Module["stdout"];error??=Module["stderr"];FS.createStandardStreams(input,output,error)},quit(){FS.initialized=false;for(var i=0;i<FS.streams.length;i++){var stream=FS.streams[i];if(!stream){continue}FS.close(stream)}},findObject(path,dontResolveLastLink){var ret=FS.analyzePath(path,dontResolveLastLink);if(!ret.exists){return null}return ret.object},analyzePath(path,dontResolveLastLink){try{var lookup=FS.lookupPath(path,{follow:!dontResolveLastLink});path=lookup.path}catch(e){}var ret={isRoot:false,exists:false,error:0,name:null,path:null,object:null,parentExists:false,parentPath:null,parentObject:null};try{var lookup=FS.lookupPath(path,{parent:true});ret.parentExists=true;ret.parentPath=lookup.path;ret.parentObject=lookup.node;ret.name=PATH.basename(path);lookup=FS.lookupPath(path,{follow:!dontResolveLastLink});ret.exists=true;ret.path=lookup.path;ret.object=lookup.node;ret.name=lookup.node.name;ret.isRoot=lookup.path==="/"}catch(e){ret.error=e.errno}return ret},createPath(parent,path,canRead,canWrite){parent=typeof parent=="string"?parent:FS.getPath(parent);var parts=path.split("/").reverse();while(parts.length){var part=parts.pop();if(!part)continue;var current=PATH.join2(parent,part);try{FS.mkdir(current)}catch(e){}parent=current}return current},createFile(parent,name,properties,canRead,canWrite){var path=PATH.join2(typeof parent=="string"?parent:FS.getPath(parent),name);var mode=FS_getMode(canRead,canWrite);return FS.create(path,mode)},createDataFile(parent,name,data,canRead,canWrite,canOwn){var path=name;if(parent){parent=typeof parent=="string"?parent:FS.getPath(parent);path=name?PATH.join2(parent,name):parent}var mode=FS_getMode(canRead,canWrite);var node=FS.create(path,mode);if(data){if(typeof data=="string"){var arr=new Array(data.length);for(var i=0,len=data.length;i<len;++i)arr[i]=data.charCodeAt(i);data=arr}FS.chmod(node,mode|146);var stream=FS.open(node,577);FS.write(stream,data,0,data.length,0,canOwn);FS.close(stream);FS.chmod(node,mode)}},createDevice(parent,name,input,output){var path=PATH.join2(typeof parent=="string"?parent:FS.getPath(parent),name);var mode=FS_getMode(!!input,!!output);FS.createDevice.major??=64;var dev=FS.makedev(FS.createDevice.major++,0);FS.registerDevice(dev,{open(stream){stream.seekable=false},close(stream){if(output?.buffer?.length){output(10)}},read(stream,buffer,offset,length,pos){var bytesRead=0;for(var i=0;i<length;i++){var result;try{result=input()}catch(e){throw new FS.ErrnoError(29)}if(result===undefined&&bytesRead===0){throw new FS.ErrnoError(6)}if(result===null||result===undefined)break;bytesRead++;buffer[offset+i]=result}if(bytesRead){stream.node.atime=Date.now()}return bytesRead},write(stream,buffer,offset,length,pos){for(var i=0;i<length;i++){try{output(buffer[offset+i])}catch(e){throw new FS.ErrnoError(29)}}if(length){stream.node.mtime=stream.node.ctime=Date.now()}return i}});return FS.mkdev(path,mode,dev)},forceLoadFile(obj){if(obj.isDevice||obj.isFolder||obj.link||obj.contents)return true;if(typeof XMLHttpRequest!="undefined"){throw new Error("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.")}else{try{obj.contents=readBinary(obj.url);obj.usedBytes=obj.contents.length}catch(e){throw new FS.ErrnoError(29)}}},createLazyFile(parent,name,url,canRead,canWrite){class LazyUint8Array{lengthKnown=false;chunks=[];get(idx){if(idx>this.length-1||idx<0){return undefined}var chunkOffset=idx%this.chunkSize;var chunkNum=idx/this.chunkSize|0;return this.getter(chunkNum)[chunkOffset]}setDataGetter(getter){this.getter=getter}cacheLength(){var xhr=new XMLHttpRequest;xhr.open("HEAD",url,false);xhr.send(null);if(!(xhr.status>=200&&xhr.status<300||xhr.status===304))throw new Error("Couldn\'t load "+url+". Status: "+xhr.status);var datalength=Number(xhr.getResponseHeader("Content-length"));var header;var hasByteServing=(header=xhr.getResponseHeader("Accept-Ranges"))&&header==="bytes";var usesGzip=(header=xhr.getResponseHeader("Content-Encoding"))&&header==="gzip";var chunkSize=1024*1024;if(!hasByteServing)chunkSize=datalength;var doXHR=(from,to)=>{if(from>to)throw new Error("invalid range ("+from+", "+to+") or no bytes requested!");if(to>datalength-1)throw new Error("only "+datalength+" bytes available! programmer error!");var xhr=new XMLHttpRequest;xhr.open("GET",url,false);if(datalength!==chunkSize)xhr.setRequestHeader("Range","bytes="+from+"-"+to);xhr.responseType="arraybuffer";if(xhr.overrideMimeType){xhr.overrideMimeType("text/plain; charset=x-user-defined")}xhr.send(null);if(!(xhr.status>=200&&xhr.status<300||xhr.status===304))throw new Error("Couldn\'t load "+url+". Status: "+xhr.status);if(xhr.response!==undefined){return new Uint8Array(xhr.response||[])}return intArrayFromString(xhr.responseText||"",true)};var lazyArray=this;lazyArray.setDataGetter(chunkNum=>{var start=chunkNum*chunkSize;var end=(chunkNum+1)*chunkSize-1;end=Math.min(end,datalength-1);if(typeof lazyArray.chunks[chunkNum]=="undefined"){lazyArray.chunks[chunkNum]=doXHR(start,end)}if(typeof lazyArray.chunks[chunkNum]=="undefined")throw new Error("doXHR failed!");return lazyArray.chunks[chunkNum]});if(usesGzip||!datalength){chunkSize=datalength=1;datalength=this.getter(0).length;chunkSize=datalength;out("LazyFiles on gzip forces download of the whole file when length is accessed")}this._length=datalength;this._chunkSize=chunkSize;this.lengthKnown=true}get length(){if(!this.lengthKnown){this.cacheLength()}return this._length}get chunkSize(){if(!this.lengthKnown){this.cacheLength()}return this._chunkSize}}if(typeof XMLHttpRequest!="undefined"){if(!ENVIRONMENT_IS_WORKER)throw"Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc";var lazyArray=new LazyUint8Array;var properties={isDevice:false,contents:lazyArray}}else{var properties={isDevice:false,url}}var node=FS.createFile(parent,name,properties,canRead,canWrite);if(properties.contents){node.contents=properties.contents}else if(properties.url){node.contents=null;node.url=properties.url}Object.defineProperties(node,{usedBytes:{get:function(){return this.contents.length}}});var stream_ops={};var keys=Object.keys(node.stream_ops);keys.forEach(key=>{var fn=node.stream_ops[key];stream_ops[key]=(...args)=>{FS.forceLoadFile(node);return fn(...args)}});function writeChunks(stream,buffer,offset,length,position){var contents=stream.node.contents;if(position>=contents.length)return 0;var size=Math.min(contents.length-position,length);if(contents.slice){for(var i=0;i<size;i++){buffer[offset+i]=contents[position+i]}}else{for(var i=0;i<size;i++){buffer[offset+i]=contents.get(position+i)}}return size}stream_ops.read=(stream,buffer,offset,length,position)=>{FS.forceLoadFile(node);return writeChunks(stream,buffer,offset,length,position)};stream_ops.mmap=(stream,length,position,prot,flags)=>{FS.forceLoadFile(node);var ptr=mmapAlloc(length);if(!ptr){throw new FS.ErrnoError(48)}writeChunks(stream,HEAP8,ptr,length,position);return{ptr,allocated:true}};node.stream_ops=stream_ops;return node}};Module["FS"]=FS;var UTF8ToString=(ptr,maxBytesToRead)=>{ptr>>>=0;return ptr?UTF8ArrayToString(HEAPU8,ptr,maxBytesToRead):""};Module["UTF8ToString"]=UTF8ToString;var SYSCALLS={DEFAULT_POLLMASK:5,calculateAt(dirfd,path,allowEmpty){if(PATH.isAbs(path)){return path}var dir;if(dirfd===-100){dir=FS.cwd()}else{var dirstream=SYSCALLS.getStreamFromFD(dirfd);dir=dirstream.path}if(path.length==0){if(!allowEmpty){throw new FS.ErrnoError(44)}return dir}return dir+"/"+path},writeStat(buf,stat){HEAP32[buf>>>2>>>0]=stat.dev;HEAP32[buf+4>>>2>>>0]=stat.mode;HEAPU32[buf+8>>>2>>>0]=stat.nlink;HEAP32[buf+12>>>2>>>0]=stat.uid;HEAP32[buf+16>>>2>>>0]=stat.gid;HEAP32[buf+20>>>2>>>0]=stat.rdev;HEAP64[buf+24>>>3]=BigInt(stat.size);HEAP32[buf+32>>>2>>>0]=4096;HEAP32[buf+36>>>2>>>0]=stat.blocks;var atime=stat.atime.getTime();var mtime=stat.mtime.getTime();var ctime=stat.ctime.getTime();HEAP64[buf+40>>>3]=BigInt(Math.floor(atime/1e3));HEAPU32[buf+48>>>2>>>0]=atime%1e3*1e3*1e3;HEAP64[buf+56>>>3]=BigInt(Math.floor(mtime/1e3));HEAPU32[buf+64>>>2>>>0]=mtime%1e3*1e3*1e3;HEAP64[buf+72>>>3]=BigInt(Math.floor(ctime/1e3));HEAPU32[buf+80>>>2>>>0]=ctime%1e3*1e3*1e3;HEAP64[buf+88>>>3]=BigInt(stat.ino);return 0},doMsync(addr,stream,len,flags,offset){if(!FS.isFile(stream.node.mode)){throw new FS.ErrnoError(43)}if(flags&2){return 0}var buffer=HEAPU8.slice(addr,addr+len);FS.msync(stream,buffer,offset,len,flags)},getStreamFromFD(fd){var stream=FS.getStreamChecked(fd);return stream},varargs:undefined,getStr(ptr){var ret=UTF8ToString(ptr);return ret}};Module["SYSCALLS"]=SYSCALLS;var INT53_MAX=9007199254740992;Module["INT53_MAX"]=INT53_MAX;var INT53_MIN=-9007199254740992;Module["INT53_MIN"]=INT53_MIN;var bigintToI53Checked=num=>num<INT53_MIN||num>INT53_MAX?NaN:Number(num);Module["bigintToI53Checked"]=bigintToI53Checked;function ___syscall_fcntl64(fd,cmd,varargs){varargs>>>=0;SYSCALLS.varargs=varargs;try{var stream=SYSCALLS.getStreamFromFD(fd);switch(cmd){case 0:{var arg=syscallGetVarargI();if(arg<0){return-28}while(FS.streams[arg]){arg++}var newStream;newStream=FS.dupStream(stream,arg);return newStream.fd}case 1:case 2:return 0;case 3:return stream.flags;case 4:{var arg=syscallGetVarargI();stream.flags|=arg;return 0}case 12:{var arg=syscallGetVarargP();var offset=0;HEAP16[arg+offset>>>1>>>0]=2;return 0}case 13:case 14:return 0}return-28}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["___syscall_fcntl64"]=___syscall_fcntl64;function ___syscall_ioctl(fd,op,varargs){varargs>>>=0;SYSCALLS.varargs=varargs;try{var stream=SYSCALLS.getStreamFromFD(fd);switch(op){case 21509:{if(!stream.tty)return-59;return 0}case 21505:{if(!stream.tty)return-59;if(stream.tty.ops.ioctl_tcgets){var termios=stream.tty.ops.ioctl_tcgets(stream);var argp=syscallGetVarargP();HEAP32[argp>>>2>>>0]=termios.c_iflag||0;HEAP32[argp+4>>>2>>>0]=termios.c_oflag||0;HEAP32[argp+8>>>2>>>0]=termios.c_cflag||0;HEAP32[argp+12>>>2>>>0]=termios.c_lflag||0;for(var i=0;i<32;i++){HEAP8[argp+i+17>>>0]=termios.c_cc[i]||0}return 0}return 0}case 21510:case 21511:case 21512:{if(!stream.tty)return-59;return 0}case 21506:case 21507:case 21508:{if(!stream.tty)return-59;if(stream.tty.ops.ioctl_tcsets){var argp=syscallGetVarargP();var c_iflag=HEAP32[argp>>>2>>>0];var c_oflag=HEAP32[argp+4>>>2>>>0];var c_cflag=HEAP32[argp+8>>>2>>>0];var c_lflag=HEAP32[argp+12>>>2>>>0];var c_cc=[];for(var i=0;i<32;i++){c_cc.push(HEAP8[argp+i+17>>>0])}return stream.tty.ops.ioctl_tcsets(stream.tty,op,{c_iflag,c_oflag,c_cflag,c_lflag,c_cc})}return 0}case 21519:{if(!stream.tty)return-59;var argp=syscallGetVarargP();HEAP32[argp>>>2>>>0]=0;return 0}case 21520:{if(!stream.tty)return-59;return-28}case 21531:{var argp=syscallGetVarargP();return FS.ioctl(stream,op,argp)}case 21523:{if(!stream.tty)return-59;if(stream.tty.ops.ioctl_tiocgwinsz){var winsize=stream.tty.ops.ioctl_tiocgwinsz(stream.tty);var argp=syscallGetVarargP();HEAP16[argp>>>1>>>0]=winsize[0];HEAP16[argp+2>>>1>>>0]=winsize[1]}return 0}case 21524:{if(!stream.tty)return-59;return 0}case 21515:{if(!stream.tty)return-59;return 0}default:return-28}}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["___syscall_ioctl"]=___syscall_ioctl;function ___syscall_openat(dirfd,path,flags,varargs){path>>>=0;varargs>>>=0;SYSCALLS.varargs=varargs;try{path=SYSCALLS.getStr(path);path=SYSCALLS.calculateAt(dirfd,path);var mode=varargs?syscallGetVarargI():0;return FS.open(path,flags,mode).fd}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["___syscall_openat"]=___syscall_openat;var __abort_js=()=>abort("");Module["__abort_js"]=__abort_js;var runtimeKeepaliveCounter=0;Module["runtimeKeepaliveCounter"]=runtimeKeepaliveCounter;var __emscripten_runtime_keepalive_clear=()=>{noExitRuntime=false;runtimeKeepaliveCounter=0};Module["__emscripten_runtime_keepalive_clear"]=__emscripten_runtime_keepalive_clear;function __mmap_js(len,prot,flags,fd,offset,allocated,addr){len>>>=0;offset=bigintToI53Checked(offset);allocated>>>=0;addr>>>=0;try{if(isNaN(offset))return 61;var stream=SYSCALLS.getStreamFromFD(fd);var res=FS.mmap(stream,len,offset,prot,flags);var ptr=res.ptr;HEAP32[allocated>>>2>>>0]=res.allocated;HEAPU32[addr>>>2>>>0]=ptr;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["__mmap_js"]=__mmap_js;function __munmap_js(addr,len,prot,flags,fd,offset){addr>>>=0;len>>>=0;offset=bigintToI53Checked(offset);try{var stream=SYSCALLS.getStreamFromFD(fd);if(prot&2){SYSCALLS.doMsync(addr,stream,len,flags,offset)}}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return-e.errno}}Module["__munmap_js"]=__munmap_js;var timers={};Module["timers"]=timers;var handleException=e=>{if(e instanceof ExitStatus||e=="unwind"){return EXITSTATUS}quit_(1,e)};Module["handleException"]=handleException;var keepRuntimeAlive=()=>noExitRuntime||runtimeKeepaliveCounter>0;Module["keepRuntimeAlive"]=keepRuntimeAlive;var _proc_exit=code=>{EXITSTATUS=code;if(!keepRuntimeAlive()){Module["onExit"]?.(code);ABORT=true}quit_(code,new ExitStatus(code))};Module["_proc_exit"]=_proc_exit;var exitJS=(status,implicit)=>{EXITSTATUS=status;_proc_exit(status)};Module["exitJS"]=exitJS;var _exit=exitJS;Module["_exit"]=_exit;var maybeExit=()=>{if(!keepRuntimeAlive()){try{_exit(EXITSTATUS)}catch(e){handleException(e)}}};Module["maybeExit"]=maybeExit;var callUserCallback=func=>{if(ABORT){return}try{func();maybeExit()}catch(e){handleException(e)}};Module["callUserCallback"]=callUserCallback;var _emscripten_get_now=()=>performance.now();Module["_emscripten_get_now"]=_emscripten_get_now;var __setitimer_js=(which,timeout_ms)=>{if(timers[which]){clearTimeout(timers[which].id);delete timers[which]}if(!timeout_ms)return 0;var id=setTimeout(()=>{delete timers[which];callUserCallback(()=>__emscripten_timeout(which,_emscripten_get_now()))},timeout_ms);timers[which]={id,timeout_ms};return 0};Module["__setitimer_js"]=__setitimer_js;var stringToUTF8=(str,outPtr,maxBytesToWrite)=>stringToUTF8Array(str,HEAPU8,outPtr,maxBytesToWrite);Module["stringToUTF8"]=stringToUTF8;var __tzset_js=function(timezone,daylight,std_name,dst_name){timezone>>>=0;daylight>>>=0;std_name>>>=0;dst_name>>>=0;var currentYear=(new Date).getFullYear();var winter=new Date(currentYear,0,1);var summer=new Date(currentYear,6,1);var winterOffset=winter.getTimezoneOffset();var summerOffset=summer.getTimezoneOffset();var stdTimezoneOffset=Math.max(winterOffset,summerOffset);HEAPU32[timezone>>>2>>>0]=stdTimezoneOffset*60;HEAP32[daylight>>>2>>>0]=Number(winterOffset!=summerOffset);var extractZone=timezoneOffset=>{var sign=timezoneOffset>=0?"-":"+";var absOffset=Math.abs(timezoneOffset);var hours=String(Math.floor(absOffset/60)).padStart(2,"0");var minutes=String(absOffset%60).padStart(2,"0");return`UTC${sign}${hours}${minutes}`};var winterName=extractZone(winterOffset);var summerName=extractZone(summerOffset);if(summerOffset<winterOffset){stringToUTF8(winterName,std_name,17);stringToUTF8(summerName,dst_name,17)}else{stringToUTF8(winterName,dst_name,17);stringToUTF8(summerName,std_name,17)}};Module["__tzset_js"]=__tzset_js;var _emscripten_date_now=()=>Date.now();Module["_emscripten_date_now"]=_emscripten_date_now;var nowIsMonotonic=1;Module["nowIsMonotonic"]=nowIsMonotonic;var checkWasiClock=clock_id=>clock_id>=0&&clock_id<=3;Module["checkWasiClock"]=checkWasiClock;function _clock_time_get(clk_id,ignored_precision,ptime){ignored_precision=bigintToI53Checked(ignored_precision);ptime>>>=0;if(!checkWasiClock(clk_id)){return 28}var now;if(clk_id===0){now=_emscripten_date_now()}else if(nowIsMonotonic){now=_emscripten_get_now()}else{return 52}var nsec=Math.round(now*1e3*1e3);HEAP64[ptime>>>3]=BigInt(nsec);return 0}Module["_clock_time_get"]=_clock_time_get;var getHeapMax=()=>4294901760;Module["getHeapMax"]=getHeapMax;var growMemory=size=>{var b=wasmMemory.buffer;var pages=(size-b.byteLength+65535)/65536|0;try{wasmMemory.grow(pages);updateMemoryViews();return 1}catch(e){}};Module["growMemory"]=growMemory;function _emscripten_resize_heap(requestedSize){requestedSize>>>=0;var oldSize=HEAPU8.length;var maxHeapSize=getHeapMax();if(requestedSize>maxHeapSize){return false}for(var cutDown=1;cutDown<=4;cutDown*=2){var overGrownHeapSize=oldSize*(1+.2/cutDown);overGrownHeapSize=Math.min(overGrownHeapSize,requestedSize+100663296);var newSize=Math.min(maxHeapSize,alignMemory(Math.max(requestedSize,overGrownHeapSize),65536));var replacement=growMemory(newSize);if(replacement){return true}}return false}Module["_emscripten_resize_heap"]=_emscripten_resize_heap;var ENV={};Module["ENV"]=ENV;var getExecutableName=()=>thisProgram||"./this.program";Module["getExecutableName"]=getExecutableName;var getEnvStrings=()=>{if(!getEnvStrings.strings){var lang=(typeof navigator=="object"&&navigator.languages&&navigator.languages[0]||"C").replace("-","_")+".UTF-8";var env={USER:"web_user",LOGNAME:"web_user",PATH:"/",PWD:"/",HOME:"/home/web_user",LANG:lang,_:getExecutableName()};for(var x in ENV){if(ENV[x]===undefined)delete env[x];else env[x]=ENV[x]}var strings=[];for(var x in env){strings.push(`${x}=${env[x]}`)}getEnvStrings.strings=strings}return getEnvStrings.strings};Module["getEnvStrings"]=getEnvStrings;var stringToAscii=(str,buffer)=>{for(var i=0;i<str.length;++i){HEAP8[buffer++>>>0]=str.charCodeAt(i)}HEAP8[buffer>>>0]=0};Module["stringToAscii"]=stringToAscii;var _environ_get=function(__environ,environ_buf){__environ>>>=0;environ_buf>>>=0;var bufSize=0;getEnvStrings().forEach((string,i)=>{var ptr=environ_buf+bufSize;HEAPU32[__environ+i*4>>>2>>>0]=ptr;stringToAscii(string,ptr);bufSize+=string.length+1});return 0};Module["_environ_get"]=_environ_get;var _environ_sizes_get=function(penviron_count,penviron_buf_size){penviron_count>>>=0;penviron_buf_size>>>=0;var strings=getEnvStrings();HEAPU32[penviron_count>>>2>>>0]=strings.length;var bufSize=0;strings.forEach(string=>bufSize+=string.length+1);HEAPU32[penviron_buf_size>>>2>>>0]=bufSize;return 0};Module["_environ_sizes_get"]=_environ_sizes_get;function _fd_close(fd){try{var stream=SYSCALLS.getStreamFromFD(fd);FS.close(stream);return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_close"]=_fd_close;var doReadv=(stream,iov,iovcnt,offset)=>{var ret=0;for(var i=0;i<iovcnt;i++){var ptr=HEAPU32[iov>>>2>>>0];var len=HEAPU32[iov+4>>>2>>>0];iov+=8;var curr=FS.read(stream,HEAP8,ptr,len,offset);if(curr<0)return-1;ret+=curr;if(curr<len)break;if(typeof offset!="undefined"){offset+=curr}}return ret};Module["doReadv"]=doReadv;function _fd_read(fd,iov,iovcnt,pnum){iov>>>=0;iovcnt>>>=0;pnum>>>=0;try{var stream=SYSCALLS.getStreamFromFD(fd);var num=doReadv(stream,iov,iovcnt);HEAPU32[pnum>>>2>>>0]=num;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_read"]=_fd_read;function _fd_seek(fd,offset,whence,newOffset){offset=bigintToI53Checked(offset);newOffset>>>=0;try{if(isNaN(offset))return 61;var stream=SYSCALLS.getStreamFromFD(fd);FS.llseek(stream,offset,whence);HEAP64[newOffset>>>3]=BigInt(stream.position);if(stream.getdents&&offset===0&&whence===0)stream.getdents=null;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_seek"]=_fd_seek;var doWritev=(stream,iov,iovcnt,offset)=>{var ret=0;for(var i=0;i<iovcnt;i++){var ptr=HEAPU32[iov>>>2>>>0];var len=HEAPU32[iov+4>>>2>>>0];iov+=8;var curr=FS.write(stream,HEAP8,ptr,len,offset);if(curr<0)return-1;ret+=curr;if(curr<len){break}if(typeof offset!="undefined"){offset+=curr}}return ret};Module["doWritev"]=doWritev;function _fd_write(fd,iov,iovcnt,pnum){iov>>>=0;iovcnt>>>=0;pnum>>>=0;try{var stream=SYSCALLS.getStreamFromFD(fd);var num=doWritev(stream,iov,iovcnt);HEAPU32[pnum>>>2>>>0]=num;return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_fd_write"]=_fd_write;function _random_get(buffer,size){buffer>>>=0;size>>>=0;try{randomFill(HEAPU8.subarray(buffer>>>0,buffer+size>>>0));return 0}catch(e){if(typeof FS=="undefined"||!(e.name==="ErrnoError"))throw e;return e.errno}}Module["_random_get"]=_random_get;var getCFunc=ident=>{var func=Module["_"+ident];return func};Module["getCFunc"]=getCFunc;var writeArrayToMemory=(array,buffer)=>{HEAP8.set(array,buffer>>>0)};Module["writeArrayToMemory"]=writeArrayToMemory;var stackAlloc=sz=>__emscripten_stack_alloc(sz);Module["stackAlloc"]=stackAlloc;var stringToUTF8OnStack=str=>{var size=lengthBytesUTF8(str)+1;var ret=stackAlloc(size);stringToUTF8(str,ret,size);return ret};Module["stringToUTF8OnStack"]=stringToUTF8OnStack;var stackSave=()=>_emscripten_stack_get_current();Module["stackSave"]=stackSave;var stackRestore=val=>__emscripten_stack_restore(val);Module["stackRestore"]=stackRestore;var ccall=(ident,returnType,argTypes,args,opts)=>{var toC={string:str=>{var ret=0;if(str!==null&&str!==undefined&&str!==0){ret=stringToUTF8OnStack(str)}return ret},array:arr=>{var ret=stackAlloc(arr.length);writeArrayToMemory(arr,ret);return ret}};function convertReturnValue(ret){if(returnType==="string"){return UTF8ToString(ret)}if(returnType==="boolean")return Boolean(ret);return ret}var func=getCFunc(ident);var cArgs=[];var stack=0;if(args){for(var i=0;i<args.length;i++){var converter=toC[argTypes[i]];if(converter){if(stack===0)stack=stackSave();cArgs[i]=converter(args[i])}else{cArgs[i]=args[i]}}}var ret=func(...cArgs);function onDone(ret){if(stack!==0)stackRestore(stack);return convertReturnValue(ret)}ret=onDone(ret);return ret};Module["ccall"]=ccall;var cwrap=(ident,returnType,argTypes,opts)=>{var numericArgs=!argTypes||argTypes.every(type=>type==="number"||type==="boolean");var numericRet=returnType!=="string";if(numericRet&&numericArgs&&!opts){return getCFunc(ident)}return(...args)=>ccall(ident,returnType,argTypes,args,opts)};Module["cwrap"]=cwrap;var FS_createPath=FS.createPath;Module["FS_createPath"]=FS_createPath;var FS_unlink=path=>FS.unlink(path);Module["FS_unlink"]=FS_unlink;var FS_createLazyFile=FS.createLazyFile;Module["FS_createLazyFile"]=FS_createLazyFile;var FS_createDevice=FS.createDevice;Module["FS_createDevice"]=FS_createDevice;FS.createPreloadedFile=FS_createPreloadedFile;FS.staticInit();Module["FS_createPath"]=FS.createPath;Module["FS_createDataFile"]=FS.createDataFile;Module["FS_createPreloadedFile"]=FS.createPreloadedFile;Module["FS_unlink"]=FS.unlink;Module["FS_createLazyFile"]=FS.createLazyFile;Module["FS_createDevice"]=FS.createDevice;MEMFS.doesNotExistError=new FS.ErrnoError(44);MEMFS.doesNotExistError.stack="<generic error, no stack>";var wasmImports={b:___syscall_fcntl64,f:___syscall_ioctl,g:___syscall_openat,t:__abort_js,j:__emscripten_runtime_keepalive_clear,n:__mmap_js,o:__munmap_js,k:__setitimer_js,p:__tzset_js,s:_clock_time_get,h:_emscripten_date_now,m:_emscripten_resize_heap,r:_environ_get,c:_environ_sizes_get,a:_fd_close,e:_fd_read,q:_fd_seek,d:_fd_write,i:_proc_exit,l:_random_get};var wasmExports;createWasm();var ___wasm_call_ctors=()=>(___wasm_call_ctors=wasmExports["v"])();var _wllama_malloc=Module["_wllama_malloc"]=(a0,a1)=>(_wllama_malloc=Module["_wllama_malloc"]=wasmExports["w"])(a0,a1);var _wllama_start=Module["_wllama_start"]=()=>(_wllama_start=Module["_wllama_start"]=wasmExports["x"])();var _wllama_action=Module["_wllama_action"]=(a0,a1)=>(_wllama_action=Module["_wllama_action"]=wasmExports["y"])(a0,a1);var _wllama_exit=Module["_wllama_exit"]=()=>(_wllama_exit=Module["_wllama_exit"]=wasmExports["z"])();var _wllama_debug=Module["_wllama_debug"]=()=>(_wllama_debug=Module["_wllama_debug"]=wasmExports["A"])();var _main=Module["_main"]=(a0,a1)=>(_main=Module["_main"]=wasmExports["B"])(a0,a1);var _emscripten_builtin_memalign=(a0,a1)=>(_emscripten_builtin_memalign=wasmExports["D"])(a0,a1);var __emscripten_timeout=(a0,a1)=>(__emscripten_timeout=wasmExports["E"])(a0,a1);var ___trap=()=>(___trap=wasmExports["F"])();var __emscripten_stack_restore=a0=>(__emscripten_stack_restore=wasmExports["G"])(a0);var __emscripten_stack_alloc=a0=>(__emscripten_stack_alloc=wasmExports["H"])(a0);var _emscripten_stack_get_current=()=>(_emscripten_stack_get_current=wasmExports["I"])();function applySignatureConversions(wasmExports){wasmExports=Object.assign({},wasmExports);var makeWrapper_ppp=f=>(a0,a1)=>f(a0,a1)>>>0;var makeWrapper_pp=f=>a0=>f(a0)>>>0;var makeWrapper_p=f=>()=>f()>>>0;wasmExports["D"]=makeWrapper_ppp(wasmExports["D"]);wasmExports["H"]=makeWrapper_pp(wasmExports["H"]);wasmExports["I"]=makeWrapper_p(wasmExports["I"]);return wasmExports}Module["addRunDependency"]=addRunDependency;Module["removeRunDependency"]=removeRunDependency;Module["ccall"]=ccall;Module["cwrap"]=cwrap;Module["FS_createPreloadedFile"]=FS_createPreloadedFile;Module["FS_unlink"]=FS_unlink;Module["FS_createPath"]=FS_createPath;Module["FS_createDevice"]=FS_createDevice;Module["FS_createDataFile"]=FS_createDataFile;Module["FS_createLazyFile"]=FS_createLazyFile;function callMain(){var entryFunction=_main;var argc=0;var argv=0;try{var ret=entryFunction(argc,argv);exitJS(ret,true);return ret}catch(e){return handleException(e)}}function run(){if(runDependencies>0){dependenciesFulfilled=run;return}preRun();if(runDependencies>0){dependenciesFulfilled=run;return}function doRun(){Module["calledRun"]=true;if(ABORT)return;initRuntime();preMain();Module["onRuntimeInitialized"]?.();var noInitialRun=Module["noInitialRun"];if(!noInitialRun)callMain();postRun()}if(Module["setStatus"]){Module["setStatus"]("Running...");setTimeout(()=>{setTimeout(()=>Module["setStatus"](""),1);doRun()},1)}else{doRun()}}if(Module["preInit"]){if(typeof Module["preInit"]=="function")Module["preInit"]=[Module["preInit"]];while(Module["preInit"].length>0){Module["preInit"].pop()()}}run();\n';

// src/worker.ts
var ProxyToWorker = class {
  logger;
  suppressNativeLog;
  taskQueue = [];
  taskId = 1;
  resultQueue = [];
  busy = false;
  // is the work loop is running?
  worker;
  pathConfig;
  multiThread;
  nbThread;
  constructor(pathConfig, nbThread = 1, suppressNativeLog, logger) {
    this.pathConfig = pathConfig;
    this.nbThread = nbThread;
    this.multiThread = nbThread > 1;
    this.logger = logger;
    this.suppressNativeLog = suppressNativeLog;
  }
  async moduleInit(ggufFiles) {
    if (!this.pathConfig["wllama.wasm"]) {
      throw new Error('"single-thread/wllama.wasm" is missing from pathConfig');
    }
    let moduleCode = this.multiThread ? WLLAMA_MULTI_THREAD_CODE : WLLAMA_SINGLE_THREAD_CODE;
    let mainModuleCode = moduleCode.replace("var Module", "var ___Module");
    const runOptions = {
      pathConfig: this.pathConfig,
      nbThread: this.nbThread
    };
    const completeCode = [
      `const RUN_OPTIONS = ${JSON.stringify(runOptions)};`,
      `function wModuleInit() { ${mainModuleCode}; return Module; }`,
      LLAMA_CPP_WORKER_CODE
    ].join(";\n\n");
    this.worker = createWorker(completeCode);
    this.worker.onmessage = this.onRecvMsg.bind(this);
    this.worker.onerror = this.logger.error;
    const res = await this.pushTask({
      verb: "module.init",
      args: [new Blob([moduleCode], { type: "text/javascript" })],
      callbackId: this.taskId++
    });
    const nativeFiles = [];
    for (const file of ggufFiles) {
      const id = await this.fileAlloc(file.name, file.blob.size);
      nativeFiles.push({ id, ...file });
    }
    await Promise.all(
      nativeFiles.map((file) => {
        return this.fileWrite(file.id, file.blob);
      })
    );
    return res;
  }
  async wllamaStart() {
    const result = await this.pushTask({
      verb: "wllama.start",
      args: [],
      callbackId: this.taskId++
    });
    const parsedResult = this.parseResult(result);
    return parsedResult;
  }
  async wllamaAction(name, body) {
    const encodedMsg = glueSerialize(body);
    const result = await this.pushTask({
      verb: "wllama.action",
      args: [name, encodedMsg],
      callbackId: this.taskId++
    });
    const parsedResult = glueDeserialize(result);
    return parsedResult;
  }
  async wllamaExit() {
    if (this.worker) {
      const result = await this.pushTask({
        verb: "wllama.exit",
        args: [],
        callbackId: this.taskId++
      });
      this.parseResult(result);
      this.worker.terminate();
    }
  }
  async wllamaDebug() {
    const result = await this.pushTask({
      verb: "wllama.debug",
      args: [],
      callbackId: this.taskId++
    });
    return JSON.parse(result);
  }
  ///////////////////////////////////////
  /**
   * Allocate a new file in heapfs
   * @returns fileId, to be used by fileWrite()
   */
  async fileAlloc(fileName, size) {
    const result = await this.pushTask({
      verb: "fs.alloc",
      args: [fileName, size],
      callbackId: this.taskId++
    });
    return result.fileId;
  }
  /**
   * Write a Blob to heapfs
   */
  async fileWrite(fileId, blob) {
    const reader = blob.stream().getReader();
    let offset = 0;
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      const size = value.byteLength;
      await this.pushTask(
        {
          verb: "fs.write",
          args: [fileId, value, offset],
          callbackId: this.taskId++
        },
        // @ts-ignore Type 'ArrayBufferLike' is not assignable to type 'ArrayBuffer'
        [value.buffer]
      );
      offset += size;
    }
  }
  /**
   * Parse JSON result returned by cpp code.
   * Throw new Error if "__exception" is present in the response
   *
   * TODO: get rid of this function once everything is migrated to Glue
   */
  parseResult(result) {
    const parsedResult = JSON.parse(result);
    if (parsedResult && parsedResult["error"]) {
      throw new Error("Unknown error, please see console.log");
    }
    return parsedResult;
  }
  /**
   * Push a new task to taskQueue
   */
  pushTask(param, buffers) {
    return new Promise((resolve, reject) => {
      this.taskQueue.push({ resolve, reject, param, buffers });
      this.runTaskLoop();
    });
  }
  /**
   * Main loop for processing tasks
   */
  async runTaskLoop() {
    if (this.busy) {
      return;
    }
    this.busy = true;
    while (true) {
      const task = this.taskQueue.shift();
      if (!task) break;
      this.resultQueue.push(task);
      this.worker.postMessage(
        task.param,
        isSafariMobile() ? void 0 : {
          transfer: task.buffers ?? []
        }
      );
    }
    this.busy = false;
  }
  /**
   * Handle messages from worker
   */
  onRecvMsg(e) {
    if (!e.data) return;
    const { verb, args } = e.data;
    if (verb && verb.startsWith("console.")) {
      if (this.suppressNativeLog) {
        return;
      }
      if (verb.endsWith("debug")) this.logger.debug(...args);
      if (verb.endsWith("log")) this.logger.log(...args);
      if (verb.endsWith("warn")) this.logger.warn(...args);
      if (verb.endsWith("error")) this.logger.error(...args);
      return;
    } else if (verb === "signal.abort") {
      this.abort(args[0]);
    }
    const { callbackId, result, err } = e.data;
    if (callbackId) {
      const idx = this.resultQueue.findIndex(
        (t) => t.param.callbackId === callbackId
      );
      if (idx !== -1) {
        const waitingTask = this.resultQueue.splice(idx, 1)[0];
        if (err) waitingTask.reject(err);
        else waitingTask.resolve(result);
      } else {
        this.logger.error(
          `Cannot find waiting task with callbackId = ${callbackId}`
        );
      }
    }
  }
  abort(text) {
    while (this.resultQueue.length > 0) {
      const waitingTask = this.resultQueue.pop();
      if (!waitingTask) break;
      waitingTask.reject(
        new Error(
          `Received abort signal from llama.cpp; Message: ${text || "(empty)"}`
        )
      );
    }
  }
};

// src/cache-manager.ts
var PREFIX_METADATA = "__metadata__";
var POLYFILL_ETAG = "polyfill_for_older_version";
var CacheManager = class {
  /**
   * Convert a given URL into file name in cache.
   *
   * Format of the file name: `${hashSHA1(fullURL)}_${fileName}`
   */
  async getNameFromURL(url) {
    return await urlToFileName(url, "");
  }
  /**
   * @deprecated Use `download()` instead
   *
   * Write a new file to cache. This will overwrite existing file.
   *
   * @param name The file name returned by `getNameFromURL()` or `list()`
   */
  async write(name, stream, metadata) {
    this.writeMetadata(name, metadata);
    return await opfsWrite(name, stream);
  }
  async download(url, options = {}) {
    const worker = createWorker(OPFS_UTILS_WORKER_CODE);
    let aborted = false;
    if (options.signal) {
      aborted = options.signal.aborted;
      const mSignal = options.signal;
      mSignal.addEventListener("abort", () => {
        aborted = true;
        worker.postMessage({ action: "download-abort" });
      });
      delete options.signal;
    }
    const metadataFileName = await urlToFileName(url, PREFIX_METADATA);
    const filename = await urlToFileName(url, "");
    return await new Promise((resolve, reject) => {
      worker.postMessage({
        action: "download",
        url,
        filename,
        metadataFileName,
        options: { headers: options.headers, aborted }
      });
      worker.onmessage = (e) => {
        if (e.data.ok) {
          worker.terminate();
          resolve();
        } else if (e.data.err) {
          worker.terminate();
          reject(e.data.err);
        } else if (e.data.progress) {
          const progress = e.data.progress;
          options.progressCallback?.(progress);
        } else {
          reject(new Error("Unknown message from worker"));
          console.error("Unknown message from worker", e.data);
        }
      };
    });
  }
  /**
   * Open a file in cache for reading
   *
   * @param nameOrURL The file name returned by `getNameFromURL()` or `list()`, or the original URL of the remote file
   * @returns Blob, or null if file does not exist
   */
  async open(nameOrURL) {
    return await opfsOpen(nameOrURL);
  }
  /**
   * Get the size of a file in stored cache
   *
   * NOTE: in case the download is stopped mid-way (i.e. user close browser tab), the file maybe corrupted, size maybe different from `metadata.originalSize`
   *
   * @param name The file name returned by `getNameFromURL()` or `list()`
   * @returns number of bytes, or -1 if file does not exist
   */
  async getSize(name) {
    return await opfsFileSize(name);
  }
  /**
   * Get metadata of a cached file
   */
  async getMetadata(name) {
    const stream = await opfsOpen(name, PREFIX_METADATA);
    const cachedSize = await this.getSize(name);
    if (!stream) {
      return cachedSize > 0 ? (
        // files created by older version of wllama doesn't have metadata, we will try to polyfill it
        {
          etag: POLYFILL_ETAG,
          originalSize: cachedSize,
          originalURL: ""
        }
      ) : (
        // if cached file not found, we don't have metadata at all
        null
      );
    }
    try {
      const meta = await new Response(stream).json();
      return meta;
    } catch (e) {
      return null;
    }
  }
  /**
   * List all files currently in cache
   */
  async list() {
    const cacheDir = await getCacheDir();
    const result = [];
    const metadataMap = {};
    for await (let [name, handler] of cacheDir.entries()) {
      if (handler.kind === "file" && name.startsWith(PREFIX_METADATA)) {
        const stream = (await handler.getFile()).stream();
        const meta = await new Response(stream).json().catch((_) => null);
        metadataMap[name.replace(PREFIX_METADATA, "")] = meta;
      }
    }
    for await (let [name, handler] of cacheDir.entries()) {
      if (handler.kind === "file" && !name.startsWith(PREFIX_METADATA)) {
        result.push({
          name,
          size: await handler.getFile().then((f) => f.size),
          metadata: metadataMap[name] || {
            // try to polyfill for old versions
            originalSize: (await handler.getFile()).size,
            originalURL: "",
            etag: ""
          }
        });
      }
    }
    return result;
  }
  /**
   * Clear all files currently in cache
   */
  async clear() {
    await this.deleteMany(() => true);
  }
  /**
   * Delete a single file in cache
   *
   * @param nameOrURL Can be either an URL or a name returned by `getNameFromURL()` or `list()`
   */
  async delete(nameOrURL) {
    const name2 = await this.getNameFromURL(nameOrURL);
    await this.deleteMany(
      (entry) => entry.name === nameOrURL || entry.name === name2
    );
  }
  /**
   * Delete multiple files in cache.
   *
   * @param predicate A predicate like `array.filter(item => boolean)`
   */
  async deleteMany(predicate) {
    const cacheDir = await getCacheDir();
    const list = await this.list();
    for (const item of list) {
      if (predicate(item)) {
        cacheDir.removeEntry(item.name);
      }
    }
  }
  /**
   * Write the metadata of the file to disk.
   *
   * This function is separated from `write()` for compatibility reason. In older version of wllama, there was no metadata for cached file, so when newer version of wllama loads a file created by older version, it will try to polyfill the metadata.
   */
  async writeMetadata(name, metadata) {
    const blob = new Blob([JSON.stringify(metadata)], { type: "text/plain" });
    await opfsWrite(name, blob.stream(), PREFIX_METADATA);
  }
};
var cache_manager_default = CacheManager;
async function opfsWrite(key, stream, prefix = "") {
  try {
    const fileName = await urlToFileName(key, prefix);
    const writable = await opfsWriteViaWorker(fileName);
    await writable.truncate(0);
    const reader = stream.getReader();
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      await writable.write(value);
    }
    await writable.close();
  } catch (e) {
    console.error("opfsWrite", e);
  }
}
async function opfsOpen(originalURLOrName, prefix = "") {
  const getFileHandler = async (fname) => {
    try {
      const cacheDir = await getCacheDir();
      const fileHandler = await cacheDir.getFileHandle(fname);
      return await fileHandler.getFile();
    } catch (e) {
      return null;
    }
  };
  let handler = await getFileHandler(originalURLOrName);
  if (handler) {
    return handler;
  }
  const fileName = await urlToFileName(originalURLOrName, prefix);
  handler = await getFileHandler(fileName);
  return handler;
}
async function opfsFileSize(originalURL, prefix = "") {
  try {
    const cacheDir = await getCacheDir();
    const fileName = await urlToFileName(originalURL, prefix);
    const fileHandler = await cacheDir.getFileHandle(fileName);
    const file = await fileHandler.getFile();
    return file.size;
  } catch (e) {
    return -1;
  }
}
async function urlToFileName(url, prefix) {
  const hashBuffer = await crypto.subtle.digest(
    "SHA-1",
    new TextEncoder().encode(url)
  );
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  const hashHex = hashArray.map((b) => b.toString(16).padStart(2, "0")).join("");
  return `${prefix}${hashHex}_${url.split("/").pop()}`;
}
async function getCacheDir() {
  const opfsRoot = await navigator.storage.getDirectory();
  const cacheDir = await opfsRoot.getDirectoryHandle("cache", { create: true });
  return cacheDir;
}
async function opfsWriteViaWorker(fileName) {
  const worker = createWorker(OPFS_UTILS_WORKER_CODE);
  let pResolve;
  let pReject;
  worker.onmessage = (e) => {
    if (e.data.ok) pResolve(null);
    else if (e.data.err) pReject(e.data.err);
  };
  const workerExec = (data) => new Promise((resolve, reject) => {
    pResolve = resolve;
    pReject = reject;
    worker.postMessage(
      data,
      isSafariMobile() ? void 0 : {
        transfer: data.value ? [data.value.buffer] : []
      }
    );
  });
  await workerExec({ open: fileName });
  return {
    truncate: async () => {
    },
    write: (value) => workerExec({ value }),
    close: async () => {
      await workerExec({ done: true });
      worker.terminate();
    }
  };
}

// src/model-manager.ts
var DEFAULT_PARALLEL_DOWNLOADS = 3;
var ModelValidationStatus = /* @__PURE__ */ ((ModelValidationStatus2) => {
  ModelValidationStatus2["VALID"] = "valid";
  ModelValidationStatus2["INVALID"] = "invalid";
  ModelValidationStatus2["DELETED"] = "deleted";
  return ModelValidationStatus2;
})(ModelValidationStatus || {});
var Model = class {
  modelManager;
  constructor(modelManager, url, savedFiles) {
    this.modelManager = modelManager;
    this.url = url;
    if (savedFiles) {
      this.files = this.getAllFiles(savedFiles);
      this.size = sumArr(this.files.map((f) => f.metadata.originalSize));
    } else {
      this.files = [];
      this.size = 0;
    }
  }
  /**
   * URL to the GGUF file (in case it contains multiple shards, the URL should point to the first shard)
   *
   * This URL will be used to identify the model in the cache. There can't be 2 models with the same URL.
   */
  url;
  /**
   * Size in bytes (total size of all shards).
   *
   * A value of -1 means the model is deleted from the cache. You must call `ModelManager.downloadModel` to re-download the model.
   */
  size;
  /**
   * List of all shards in the cache, sorted by original URL (ascending order)
   */
  files;
  /**
   * Open and get a list of all shards as Blobs
   */
  async open() {
    if (this.size === -1) {
      throw new WllamaError(
        `Model is deleted from the cache; Call ModelManager.downloadModel to re-download the model`,
        "load_error"
      );
    }
    const blobs = [];
    for (const file of this.files) {
      const blob = await this.modelManager.cacheManager.open(file.name);
      if (!blob) {
        throw new Error(
          `Failed to open file ${file.name}; Hint: the model may be invalid, please refresh it`
        );
      }
      blobs.push(blob);
    }
    return blobs;
  }
  /**
   * Validate the model files.
   *
   * If the model is invalid, the model manager will not be able to use it. You must call `refresh` to re-download the model.
   *
   * Cases that model is invalid:
   * - The model is deleted from the cache
   * - The model files are missing (or the download is interrupted)
   */
  validate() {
    const nbShards = ModelManager.parseModelUrl(this.url).length;
    if (this.size === -1) {
      return "deleted" /* DELETED */;
    }
    if (this.size < 16 || this.files.length !== nbShards) {
      return "invalid" /* INVALID */;
    }
    for (const file of this.files) {
      if (!file.metadata || file.metadata.originalSize !== file.size) {
        return "invalid" /* INVALID */;
      }
    }
    return "valid" /* VALID */;
  }
  /**
   * In case the model is invalid, call this function to re-download the model
   */
  async refresh(options = {}) {
    const urls = ModelManager.parseModelUrl(this.url);
    const works = urls.map((url, index) => ({
      url,
      index
    }));
    this.modelManager.logger.debug("Downloading model files:", urls);
    const nParallel = this.modelManager.params.parallelDownloads ?? DEFAULT_PARALLEL_DOWNLOADS;
    const totalSize = await this.getTotalDownloadSize(urls);
    const loadedSize = [];
    const worker = async () => {
      while (works.length > 0) {
        const w = works.shift();
        if (!w) break;
        await this.modelManager.cacheManager.download(w.url, {
          ...options,
          progressCallback: ({ loaded }) => {
            loadedSize[w.index] = loaded;
            options.progressCallback?.({
              loaded: sumArr(loadedSize),
              total: totalSize
            });
          }
        });
      }
    };
    const promises = [];
    for (let i = 0; i < nParallel; i++) {
      promises.push(worker());
      loadedSize.push(0);
    }
    await Promise.all(promises);
    this.files = this.getAllFiles(await this.modelManager.cacheManager.list());
    this.size = this.files.reduce((acc, f) => acc + f.metadata.originalSize, 0);
  }
  /**
   * Remove the model from the cache
   */
  async remove() {
    this.files = this.getAllFiles(await this.modelManager.cacheManager.list());
    await this.modelManager.cacheManager.deleteMany(
      (f) => !!this.files.find((file) => file.name === f.name)
    );
    this.size = -1;
  }
  getAllFiles(savedFiles) {
    const allUrls = new Set(ModelManager.parseModelUrl(this.url));
    const allFiles = [];
    for (const url of allUrls) {
      const file = savedFiles.find((f) => f.metadata.originalURL === url);
      if (!file) {
        throw new Error(`Model file not found: ${url}`);
      }
      allFiles.push(file);
    }
    allFiles.sort(
      (a, b) => a.metadata.originalURL.localeCompare(b.metadata.originalURL)
    );
    return allFiles;
  }
  async getTotalDownloadSize(urls) {
    const responses = await Promise.all(
      urls.map((url) => fetch(url, { method: "HEAD" }))
    );
    const sizes = responses.map(
      (res) => Number(res.headers.get("content-length") || "0")
    );
    return sumArr(sizes);
  }
};
var ModelManager = class _ModelManager {
  // The CacheManager singleton, can be accessed by user
  cacheManager;
  params;
  logger;
  constructor(params = {}) {
    this.cacheManager = params.cacheManager || new cache_manager_default();
    this.params = params;
    this.logger = params.logger || console;
  }
  /**
   * Parses a model URL and returns an array of URLs based on the following patterns:
   * - If the input URL is an array, it returns the array itself.
   * - If the input URL is a string in the `gguf-split` format, it returns an array containing the URL of each shard in ascending order.
   * - Otherwise, it returns an array containing the input URL as a single element array.
   * @param modelUrl URL or list of URLs
   */
  static parseModelUrl(modelUrl) {
    if (Array.isArray(modelUrl)) {
      return modelUrl;
    }
    const urlPartsRegex = /-(\d{5})-of-(\d{5})\.gguf$/;
    const matches = modelUrl.match(urlPartsRegex);
    if (!matches) {
      return [modelUrl];
    }
    const baseURL = modelUrl.replace(urlPartsRegex, "");
    const total = matches[2];
    const paddedShardIds = Array.from(
      { length: Number(total) },
      (_, index) => (index + 1).toString().padStart(5, "0")
    );
    return paddedShardIds.map(
      (current) => `${baseURL}-${current}-of-${total}.gguf`
    );
  }
  /**
   * Get all models in the cache
   */
  async getModels(opts = {}) {
    const cachedFiles = await this.cacheManager.list();
    let models = [];
    for (const file of cachedFiles) {
      const shards = _ModelManager.parseModelUrl(file.metadata.originalURL);
      const isFirstShard = shards.length === 1 || shards[0] === file.metadata.originalURL;
      if (isFirstShard) {
        models.push(new Model(this, file.metadata.originalURL, cachedFiles));
      }
    }
    if (!opts.includeInvalid) {
      models = models.filter(
        (m) => m.validate() === "valid" /* VALID */
      );
    }
    return models;
  }
  /**
   * Download a model from the given URL.
   *
   * The URL must end with `.gguf`
   */
  async downloadModel(url, options = {}) {
    if (!url.endsWith(".gguf")) {
      throw new WllamaError(
        `Invalid model URL: ${url}; URL must ends with ".gguf"`,
        "download_error"
      );
    }
    const model = new Model(this, url, void 0);
    const validity = model.validate();
    if (validity !== "valid" /* VALID */) {
      await model.refresh(options);
    }
    return model;
  }
  /**
   * Get a model from the cache or download it if it's not available.
   */
  async getModelOrDownload(url, options = {}) {
    const models = await this.getModels();
    const model = models.find((m) => m.url === url);
    if (model) {
      options.progressCallback?.({ loaded: model.size, total: model.size });
      return model;
    }
    return this.downloadModel(url, options);
  }
  /**
   * Remove all models from the cache
   */
  async clear() {
    await this.cacheManager.clear();
  }
};

// src/wllama.ts
var HF_MODEL_ID_REGEX = /^([a-zA-Z0-9_\-\.]+)\/([a-zA-Z0-9_\-\.]+)$/;
var HF_MODEL_ID_REGEX_EXPLAIN = "Hugging Face model ID is incorrect. Only regular alphanumeric characters, '-', '.' and '_' supported";
var LoggerWithoutDebug = {
  ...console,
  debug: () => {
  }
};
var WllamaError = class extends Error {
  type;
  constructor(message, type = "unknown_error") {
    super(message);
    this.type = type;
  }
};
var WllamaAbortError = class extends Error {
  name = "AbortError";
  constructor() {
    super("Operation aborted");
  }
};
var Wllama = class {
  // The CacheManager and ModelManager are singleton, can be accessed by user
  cacheManager;
  modelManager;
  proxy = null;
  config;
  pathConfig;
  useMultiThread = false;
  nbThreads = 1;
  useEmbeddings = false;
  // available when loaded
  loadedContextInfo = null;
  bosToken = -1;
  eosToken = -1;
  eotToken = -1;
  eogTokens = /* @__PURE__ */ new Set();
  addBosToken = false;
  addEosToken = false;
  chatTemplate;
  metadata;
  samplingConfig = {};
  hasEncoder = false;
  decoderStartToken = -1;
  nCachedTokens = 0;
  constructor(pathConfig, wllamaConfig = {}) {
    checkEnvironmentCompatible();
    if (!pathConfig) throw new WllamaError("AssetsPathConfig is required");
    this.pathConfig = pathConfig;
    this.config = wllamaConfig;
    this.cacheManager = wllamaConfig.cacheManager ?? new cache_manager_default();
    this.modelManager = wllamaConfig.modelManager ?? new ModelManager({
      cacheManager: this.cacheManager,
      logger: wllamaConfig.logger ?? console,
      parallelDownloads: wllamaConfig.parallelDownloads,
      allowOffline: wllamaConfig.allowOffline
    });
  }
  logger() {
    return this.config.logger ?? console;
  }
  checkModelLoaded() {
    if (!this.isModelLoaded()) {
      throw new WllamaError(
        "loadModel() is not yet called",
        "model_not_loaded"
      );
    }
  }
  /**
   * Check if the model is loaded via `loadModel()`
   */
  isModelLoaded() {
    return !!this.proxy && !!this.metadata;
  }
  /**
   * Get token ID associated to BOS (begin of sentence) token.
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns -1 if the model is not loaded.
   */
  getBOS() {
    return this.bosToken;
  }
  /**
   * Get token ID associated to EOS (end of sentence) token.
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns -1 if the model is not loaded.
   */
  getEOS() {
    return this.eosToken;
  }
  /**
   * Get token ID associated to EOT (end of turn) token.
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns -1 if the model is not loaded.
   */
  getEOT() {
    return this.eotToken;
  }
  /**
   * Check if a given token is end-of-generation token (e.g. EOS, EOT, etc.)
   *
   * @param token the token ID to be checked
   * @returns true if the token is EOS, EOT, or any other end-of-generation tokens
   */
  isTokenEOG(token) {
    return token === this.eosToken || token === this.eotToken || this.eogTokens.has(token);
  }
  /**
   * Get token ID associated to token used by decoder, to start generating output sequence(only usable for encoder-decoder architecture). In other words, encoder uses normal BOS and decoder uses this token.
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns -1 if the model is not loaded.
   */
  getDecoderStartToken() {
    return this.decoderStartToken;
  }
  /**
   * Get model hyper-parameters and metadata
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns ModelMetadata
   */
  getModelMetadata() {
    this.checkModelLoaded();
    return this.metadata;
  }
  /**
   * Check if we're currently using multi-thread build.
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns true if multi-thread is used.
   */
  isMultithread() {
    this.checkModelLoaded();
    return this.useMultiThread;
  }
  /**
   * Get number of threads used in the current context.
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns number of threads
   */
  getNumThreads() {
    this.checkModelLoaded();
    return this.useMultiThread ? this.nbThreads : 1;
  }
  /**
   * Check if the current model uses encoder-decoder architecture
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns true if multi-thread is used.
   */
  isEncoderDecoderArchitecture() {
    this.checkModelLoaded();
    return this.hasEncoder;
  }
  /**
   * Must we add BOS token to the tokenized sequence?
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns true if BOS token must be added to the sequence
   */
  mustAddBosToken() {
    this.checkModelLoaded();
    return this.addBosToken;
  }
  /**
   * Must we add EOS token to the tokenized sequence?
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns true if EOS token must be added to the sequence
   */
  mustAddEosToken() {
    this.checkModelLoaded();
    return this.addEosToken;
  }
  /**
   * Get the jinja chat template comes with the model. It only available if the original model (before converting to gguf) has the template in `tokenizer_config.json`
   *
   * NOTE: This can only being used after `loadModel` is called.
   *
   * @returns the jinja template. null if there is no template in gguf
   */
  getChatTemplate() {
    this.checkModelLoaded();
    return this.chatTemplate ?? null;
  }
  /**
   * Load model from a given URL (or a list of URLs, in case the model is splitted into smaller files)
   * - If the model already been downloaded (via `downloadModel()`), then we will use the cached model
   * - Else, we download the model from internet
   * @param modelUrl URL to the GGUF file. If the model is splitted, pass the URL to the first shard.
   * @param config
   */
  async loadModelFromUrl(modelUrl, config = {}) {
    const url = isString(modelUrl) ? modelUrl : modelUrl[0];
    const useCache = config.useCache ?? true;
    const model = useCache ? await this.modelManager.getModelOrDownload(url, config) : await this.modelManager.downloadModel(url, config);
    const blobs = await model.open();
    return await this.loadModel(blobs, config);
  }
  /**
   * Load model from a given Hugging Face model ID and file path.
   *
   * @param modelId The HF model ID, for example: 'ggml-org/models'
   * @param filePath The GGUF file path, for example: 'tinyllamas/stories15M-q4_0.gguf'
   * @param config
   */
  async loadModelFromHF(modelId, filePath, config = {}) {
    if (!modelId.match(HF_MODEL_ID_REGEX)) {
      throw new WllamaError(HF_MODEL_ID_REGEX_EXPLAIN, "download_error");
    }
    if (!filePath.endsWith(".gguf")) {
      throw new WllamaError("Only GGUF file is supported", "download_error");
    }
    return await this.loadModelFromUrl(
      `https://huggingface.co/${modelId}/resolve/main/${filePath}`,
      config
    );
  }
  /**
   * Load model from a given list of Blob.
   *
   * You can pass multiple buffers into the function (in case the model contains multiple shards).
   *
   * @param ggufBlobsOrModel Can be either list of Blobs (in case you use local file), or a Model object (in case you use ModelManager)
   * @param config LoadModelConfig
   */
  async loadModel(ggufBlobsOrModel, config = {}) {
    const blobs = ggufBlobsOrModel instanceof Model ? await ggufBlobsOrModel.open() : [...ggufBlobsOrModel];
    if (blobs.some((b) => b.size === 0)) {
      throw new WllamaError(
        "Input model (or splits) must be non-empty Blob or File",
        "load_error"
      );
    }
    sortFileByShard(blobs);
    if (this.proxy) {
      throw new WllamaError("Module is already initialized", "load_error");
    }
    const supportMultiThread = await isSupportMultiThread();
    if (!supportMultiThread) {
      this.logger().warn(
        "Multi-threads are not supported in this environment, falling back to single-thread"
      );
    }
    const hasPathMultiThread = !!this.pathConfig["multi-thread/wllama.wasm"];
    if (!hasPathMultiThread) {
      this.logger().warn(
        'Missing paths to "multi-thread/wllama.wasm", falling back to single-thread'
      );
    }
    const hwConccurency = Math.floor((navigator.hardwareConcurrency || 1) / 2);
    const nbThreads = config.n_threads ?? hwConccurency;
    this.nbThreads = nbThreads;
    this.useMultiThread = supportMultiThread && hasPathMultiThread && nbThreads > 1;
    const mPathConfig = this.useMultiThread ? {
      "wllama.wasm": absoluteUrl(
        this.pathConfig["multi-thread/wllama.wasm"]
      )
    } : {
      "wllama.wasm": absoluteUrl(
        this.pathConfig["single-thread/wllama.wasm"]
      )
    };
    this.proxy = new ProxyToWorker(
      mPathConfig,
      this.useMultiThread ? nbThreads : 1,
      this.config.suppressNativeLog ?? false,
      this.logger()
    );
    const modelFiles = blobs.map((blob, i) => ({
      name: `model-${i}.gguf`,
      blob
    }));
    await this.proxy.moduleInit(modelFiles);
    const startResult = await this.proxy.wllamaStart();
    if (!startResult.success) {
      throw new WllamaError(
        `Error while calling start function, result = ${startResult}`
      );
    }
    const loadResult = await this.proxy.wllamaAction("load", {
      _name: "load_req",
      use_mmap: true,
      use_mlock: true,
      n_gpu_layers: 0,
      // not supported for now
      seed: config.seed || Math.floor(Math.random() * 1e5),
      n_ctx: config.n_ctx || 1024,
      n_threads: this.useMultiThread ? nbThreads : 1,
      n_ctx_auto: false,
      // not supported for now
      model_paths: modelFiles.map((f) => `models/${f.name}`),
      embeddings: config.embeddings,
      offload_kqv: config.offload_kqv,
      n_batch: config.n_batch,
      pooling_type: config.pooling_type,
      rope_scaling_type: config.rope_scaling_type,
      rope_freq_base: config.rope_freq_base,
      rope_freq_scale: config.rope_freq_scale,
      yarn_ext_factor: config.yarn_ext_factor,
      yarn_attn_factor: config.yarn_attn_factor,
      yarn_beta_fast: config.yarn_beta_fast,
      yarn_beta_slow: config.yarn_beta_slow,
      yarn_orig_ctx: config.yarn_orig_ctx,
      cache_type_k: config.cache_type_k,
      cache_type_v: config.cache_type_v,
      ...config
    });
    const loadedCtxInfo = {
      ...loadResult,
      metadata: {}
    };
    for (let i = 0; i < loadResult.metadata_key.length; i++) {
      loadedCtxInfo.metadata[loadResult.metadata_key[i]] = loadResult.metadata_val[i];
    }
    this.bosToken = loadedCtxInfo.token_bos;
    this.eosToken = loadedCtxInfo.token_eos;
    this.eotToken = loadedCtxInfo.token_eot;
    this.useEmbeddings = !!config.embeddings;
    this.metadata = {
      hparams: {
        nVocab: loadedCtxInfo.n_vocab,
        nCtxTrain: loadedCtxInfo.n_ctx_train,
        nEmbd: loadedCtxInfo.n_embd,
        nLayer: loadedCtxInfo.n_layer
      },
      meta: loadedCtxInfo.metadata
    };
    this.hasEncoder = !!loadedCtxInfo.has_encoder;
    this.decoderStartToken = loadedCtxInfo.token_decoder_start;
    this.addBosToken = loadedCtxInfo.add_bos_token;
    this.addEosToken = loadedCtxInfo.add_eos_token;
    this.chatTemplate = loadedCtxInfo.metadata["tokenizer.chat_template"];
    this.loadedContextInfo = loadedCtxInfo;
    this.eogTokens = new Set(loadedCtxInfo.list_tokens_eog);
    this.logger().debug({ loadedCtxInfo });
  }
  getLoadedContextInfo() {
    this.checkModelLoaded();
    if (!this.loadedContextInfo) {
      throw new WllamaError("Loaded context info is not available");
    }
    return { ...this.loadedContextInfo };
  }
  //////////////////////////////////////////////
  // High level API
  /**
   * Calculate embedding vector for a given text.
   * By default, BOS and EOS tokens will be added automatically. You can use the "skipBOS" and "skipEOS" option to disable it.
   * @param text Input text
   * @returns An embedding vector
   */
  async createEmbedding(text, options = {}) {
    this.checkModelLoaded();
    const opt = {
      skipBOS: false,
      skipEOS: false,
      ...options
    };
    await this.samplingInit(this.samplingConfig);
    await this.kvClear();
    const tokens = await this.tokenize(text);
    if (this.bosToken && !opt.skipBOS) {
      tokens.unshift(this.bosToken);
    }
    if (this.eosToken && !opt.skipEOS) {
      tokens.push(this.eosToken);
    }
    const result = await this.embeddings(tokens);
    return result;
  }
  async createChatCompletion(messages, options) {
    const prompt = await this.formatChat(messages, true);
    return options.stream ? await this.createCompletionGenerator(prompt, options) : await this.createCompletion(prompt, { ...options, stream: false });
  }
  async createCompletion(prompt, options) {
    return options.stream ? await this.createCompletionGenerator(prompt, options) : await this.createCompletionImpl(prompt, { ...options, stream: false });
  }
  /**
   * Private implementation of createCompletion
   */
  async createCompletionImpl(prompt, options) {
    this.checkModelLoaded();
    this.samplingConfig = options.sampling ?? {};
    await this.samplingInit(this.samplingConfig);
    const stopTokens = new Set(options.stopTokens ?? []);
    let tokens = Array.isArray(prompt) ? prompt : await this.tokenize(prompt, true);
    if (this.addBosToken && tokens[0] !== this.bosToken) {
      tokens.unshift(this.bosToken);
    }
    if (options.useCache) {
      tokens = await this.computeNonCachedTokens(tokens);
    } else {
      await this.kvClear();
    }
    await this.samplingAccept(tokens);
    if (this.isEncoderDecoderArchitecture()) {
      await this.encode(tokens);
      await this.decode([this.getDecoderStartToken()], {});
    } else {
      await this.decode(tokens, {});
    }
    let outBuf = new Uint8Array();
    let abort = false;
    const abortSignalFn = () => {
      abort = true;
    };
    for (let i = 0; i < (options.nPredict ?? Infinity); i++) {
      const sampled = await this.samplingSample();
      if (this.isTokenEOG(sampled.token) || stopTokens.has(sampled.token)) {
        break;
      }
      outBuf = joinBuffers([outBuf, sampled.piece]);
      if (options.onNewToken) {
        options.onNewToken(sampled.token, sampled.piece, bufToText(outBuf), {
          abortSignal: abortSignalFn
          // legacy
        });
      }
      if (abort || options.abortSignal?.aborted) {
        break;
      }
      await this.samplingAccept([sampled.token]);
      await this.decode([sampled.token], {});
    }
    return bufToText(outBuf);
  }
  /**
   * Same with `createCompletion`, but returns an async iterator instead.
   */
  createCompletionGenerator(prompt, options) {
    return new Promise((resolve, reject) => {
      const createGenerator = cbToAsyncIter(
        (callback) => {
          this.createCompletionImpl(prompt, {
            ...options,
            onNewToken: (token, piece, currentText) => {
              callback({ token, piece, currentText }, false);
            }
          }).catch(reject).then(() => {
            callback(void 0, true);
          });
        }
      );
      resolve(createGenerator());
    });
  }
  //////////////////////////////////////////////
  // Low level API
  /**
   * Create or reset the ctx_sampling
   * @param config
   * @param pastTokens In case re-initializing the ctx_sampling, you can re-import past tokens into the new context
   */
  async samplingInit(config, pastTokens = []) {
    this.checkModelLoaded();
    this.samplingConfig = config;
    const result = await this.proxy.wllamaAction(
      "sampling_init",
      {
        _name: "sint_req",
        ...config,
        tokens: pastTokens
      }
    );
    if (!result.success) {
      throw new WllamaError("Failed to initialize sampling");
    }
  }
  /**
   * Get a list of pieces in vocab.
   * NOTE: This function is slow, should only be used once.
   * @returns A list of Uint8Array. The nth element in the list associated to nth token in vocab
   */
  async getVocab() {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "get_vocab",
      {
        _name: "gvoc_req"
      }
    );
    return result.vocab;
  }
  /**
   * Lookup to see if a token exist in vocab or not. Useful for searching special tokens like "<|im_start|>"
   * NOTE: It will match the whole token, so do not use it as a replacement for tokenize()
   * @param piece
   * @returns Token ID associated to the given piece. Returns -1 if cannot find the token.
   */
  async lookupToken(piece) {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "lookup_token",
      {
        _name: "lkup_req",
        piece
      }
    );
    if (!result.success) {
      return -1;
    } else {
      return result.token;
    }
  }
  /**
   * Convert a given text to list of tokens
   * @param text
   * @param special Should split special tokens?
   * @returns List of token ID
   */
  async tokenize(text, special = true) {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "tokenize",
      {
        _name: "tokn_req",
        text,
        special: !!special
      }
    );
    return result.tokens;
  }
  async detokenize(tokens, returnString = false) {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "detokenize",
      {
        _name: "dtkn_req",
        tokens
      }
    );
    return returnString ? bufToText(result.buffer) : result.buffer;
  }
  /**
   * Run llama_decode()
   * @param tokens A list of tokens to be decoded
   * @param options Additional options
   * @returns n_past (number of tokens so far in the sequence)
   */
  async decode(tokens, options) {
    this.checkModelLoaded();
    if (this.useEmbeddings) {
      throw new WllamaError(
        "embeddings is enabled. Use wllama.setOptions({ embeddings: false }) to disable it."
      );
    }
    if (tokens.length === 0) {
      return {
        nPast: this.nCachedTokens
      };
    }
    if (this.nCachedTokens + tokens.length > this.loadedContextInfo.n_ctx) {
      throw new WllamaError(
        "Running out of context cache. Please increase n_ctx when loading the model",
        "kv_cache_full"
      );
    }
    const batches = this.breakTokensIntoBatches(
      tokens,
      this.loadedContextInfo.n_batch
    );
    let result;
    for (let i = 0; i < batches.length; i++) {
      if (options?.abortSignal?.aborted) {
        throw new WllamaAbortError();
      }
      const isNotLast = batches.length > 1 && i < batches.length - 1;
      result = await this.proxy.wllamaAction("decode", {
        _name: "deco_req",
        tokens: batches[i],
        skip_logits: options.skipLogits || isNotLast
      });
      if (result.error) {
        throw new WllamaError(result.error);
      } else if (!result.success) {
        throw new WllamaError("Cannot encode, unknown error");
      }
    }
    this.nCachedTokens = result.n_past;
    return { nPast: result.n_past };
  }
  /**
   * Run llama_encode()
   * @param tokens A list of tokens to be encoded
   * @param options Additional options
   * @returns n_past (number of tokens so far in the sequence)
   */
  async encode(tokens, options) {
    this.checkModelLoaded();
    if (!this.hasEncoder) {
      throw new WllamaError(
        "This model does not use encoder-decoder architecture.",
        "inference_error"
      );
    }
    if (this.useEmbeddings) {
      throw new WllamaError(
        "embeddings is enabled. Use wllama.setOptions({ embeddings: false }) to disable it.",
        "inference_error"
      );
    }
    if (tokens.length === 0) {
      return {
        nPast: this.nCachedTokens
      };
    }
    if (this.nCachedTokens + tokens.length > this.loadedContextInfo.n_ctx) {
      throw new WllamaError(
        "Running out of context cache. Please increase n_ctx when loading the model",
        "kv_cache_full"
      );
    }
    const batches = this.breakTokensIntoBatches(
      tokens,
      this.loadedContextInfo.n_batch
    );
    let result;
    for (let i = 0; i < batches.length; i++) {
      if (options?.abortSignal?.aborted) {
        throw new WllamaAbortError();
      }
      result = await this.proxy.wllamaAction("encode", {
        _name: "enco_req",
        tokens: batches[i]
      });
      if (result.error) {
        throw new WllamaError(result.error);
      } else if (!result.success) {
        throw new WllamaError("Cannot encode, unknown error");
      }
    }
    this.nCachedTokens = result.n_past;
    return { nPast: result.n_past };
  }
  breakTokensIntoBatches(tokens, maxBatchSize) {
    const batches = [];
    for (let i = 0; i < tokens.length; i += maxBatchSize) {
      batches.push(tokens.slice(i, i + maxBatchSize));
    }
    return batches;
  }
  /**
   * Sample a new token (remember to samplingInit() at least once before calling this function)
   * @returns the token ID and its detokenized value (which maybe an unfinished unicode)
   */
  async samplingSample() {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "sampling_sample",
      {
        _name: "ssam_req"
      }
    );
    return {
      piece: result.piece,
      token: result.token
    };
  }
  /**
   * Accept and save a new token to ctx_sampling
   * @param tokens
   */
  async samplingAccept(tokens) {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "sampling_accept",
      {
        _name: "sacc_req",
        tokens
      }
    );
    if (!result.success) {
      throw new WllamaError("samplingAccept unknown error");
    }
  }
  /**
   * Get softmax-ed probability of logits, can be used for custom sampling
   * @param topK Get top K tokens having highest logits value. If topK == -1, we return all n_vocab logits, but this is not recommended because it's slow.
   */
  async getLogits(topK = 40) {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "get_logits",
      {
        _name: "glog_req",
        top_k: topK
      }
    );
    const logits = [];
    for (let i = 0; i < result.tokens.length; i++) {
      logits.push({
        token: result.tokens[i],
        p: result.probs[i]
      });
    }
    return logits;
  }
  /**
   * Calculate embeddings for a given list of tokens. Output vector is always normalized
   * @param tokens
   * @returns A list of number represents an embedding vector of N dimensions
   */
  async embeddings(tokens) {
    this.checkModelLoaded();
    if (!this.useEmbeddings) {
      throw new WllamaError(
        "embeddings is disabled. Use wllama.setOptions({ embeddings: true }) to enable it.",
        "inference_error"
      );
    }
    if (this.nCachedTokens > 0) {
      this.logger().warn(
        "Embeddings: KV cache is not empty, this may produce incorrect results"
      );
    }
    if (this.nCachedTokens + tokens.length > this.loadedContextInfo.n_ctx) {
      throw new WllamaError(
        "Running out of context cache. Please increase n_ctx when loading the model",
        "kv_cache_full"
      );
    }
    if (tokens.length > this.loadedContextInfo.n_batch) {
      throw new WllamaError(
        "Embedding tokens does not fit into batch. Please increase n_batch when loading the model",
        "inference_error"
      );
    }
    if (tokens.length > this.loadedContextInfo.n_ubatch) {
      throw new WllamaError(
        "Embedding tokens does not fit into physical batch. Please increase n_ubatch when loading the model",
        "inference_error"
      );
    }
    const result = await this.proxy.wllamaAction(
      "embeddings",
      {
        _name: "gemb_req",
        tokens
      }
    );
    if (!result.success) {
      throw new WllamaError("embeddings unknown error");
    } else {
      return result.embeddings;
    }
  }
  /**
   * Remove and shift some tokens from KV cache.
   * Keep n_keep, remove n_discard then shift the rest
   * @param nKeep
   * @param nDiscard
   */
  async kvRemove(nKeep, nDiscard) {
    this.checkModelLoaded();
    if (nDiscard === 0) return;
    const result = await this.proxy.wllamaAction(
      "kv_remove",
      {
        _name: "kvcr_req",
        n_keep: nKeep,
        n_discard: nDiscard
      }
    );
    if (!result.success) {
      throw new WllamaError("kvRemove unknown error");
    }
    this.nCachedTokens -= nDiscard;
  }
  /**
   * Clear all tokens in KV cache
   */
  async kvClear() {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "kv_clear",
      {
        _name: "kvcc_req"
      }
    );
    if (!result.success) {
      throw new WllamaError("kvClear unknown error");
    }
    this.nCachedTokens = 0;
  }
  /**
   * Save session to file (virtual file system)
   * TODO: add ability to download the file
   * @param filePath
   * @returns List of tokens saved to the file
   */
  // async sessionSave(filePath: string): Promise<{ tokens: number[] }> {
  //   this.checkModelLoaded();
  //   const result = await this.proxy.wllamaAction('session_save', {
  //     session_path: filePath,
  //   });
  //   return result;
  // }
  /**
   * Load session from file (virtual file system)
   * TODO: add ability to download the file
   * @param filePath
   */
  // async sessionLoad(filePath: string): Promise<void> {
  //   this.checkModelLoaded();
  //   const result = await this.proxy.wllamaAction('session_load', {
  //     session_path: filePath,
  //   });
  //   if (result.error) {
  //     throw new WllamaError(result.error);
  //   } else if (!result.success) {
  //     throw new WllamaError('sessionLoad unknown error');
  //   }
  //   const cachedTokens = await this.getCachedTokens();
  //   this.nCachedTokens = cachedTokens.length;
  // }
  /**
   * Apply chat template to a list of messages
   *
   * @param messages list of messages
   * @param addAssistant whether to add assistant prompt at the end
   * @param template (optional) custom template, see llama-server --chat-template argument for more details
   * @returns formatted chat
   */
  async formatChat(messages, addAssistant, template) {
    this.checkModelLoaded();
    const roles = messages.map((m) => m.role);
    const contents = messages.map((m) => m.content);
    const result = await this.proxy.wllamaAction(
      "chat_format",
      {
        _name: "cfmt_req",
        roles,
        contents,
        tmpl: template,
        add_ass: addAssistant
      }
    );
    if (!result.success) {
      throw new WllamaError("formatChat unknown error");
    }
    return result.formatted_chat;
  }
  /**
   * Set options for underlaying llama_context
   */
  async setOptions(opt) {
    this.checkModelLoaded();
    await this.proxy.wllamaAction("set_options", {
      _name: "opti_req",
      ...opt
    });
    this.useEmbeddings = opt.embeddings;
  }
  /**
   * Unload the model and free all memory.
   *
   * Note: This function will NOT crash if model is not yet loaded
   */
  async exit() {
    await this.proxy?.wllamaExit();
    this.proxy = null;
  }
  /**
   * get debug info
   */
  async _getDebugInfo() {
    this.checkModelLoaded();
    return await this.proxy.wllamaDebug();
  }
  /**
   * benchmark function, only used internally
   */
  async _testBenchmark(type, nSamples) {
    this.checkModelLoaded();
    return await this.proxy.wllamaAction(
      "test_benchmark",
      {
        _name: "tben_req",
        type,
        n_samples: nSamples
      }
    );
  }
  /**
   * perplexity function, only used internally
   */
  async _testPerplexity(tokens) {
    this.checkModelLoaded();
    return await this.proxy.wllamaAction(
      "test_perplexity",
      {
        _name: "tper_req",
        tokens
      }
    );
  }
  ///// Prompt cache utils /////
  async getCachedTokens() {
    this.checkModelLoaded();
    const result = await this.proxy.wllamaAction(
      "current_status",
      {
        _name: "stat_req"
      }
    );
    return result.tokens;
  }
  /**
   * Compare the input sequence and cachedToken, then return the part that is not in cache.
   * This function also remove mismatch part in cache (via kvRemove)
   */
  async computeNonCachedTokens(seq) {
    const cachedTokens = await this.getCachedTokens();
    let nKeep = 0;
    for (; nKeep < Math.min(cachedTokens.length, seq.length); nKeep++) {
      if (cachedTokens[nKeep] !== seq[nKeep]) {
        break;
      }
    }
    this.logger().debug(`Cache nKeep=${nKeep}`);
    await this.kvRemove(nKeep, -1);
    return seq.slice(nKeep, seq.length);
  }
  // TODO: add current_status
};
export {
  LoggerWithoutDebug,
  Model,
  ModelManager,
  ModelValidationStatus,
  POLYFILL_ETAG,
  Wllama,
  WllamaAbortError,
  WllamaError
};
