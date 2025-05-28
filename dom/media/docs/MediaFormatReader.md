# MediaFormatReader

The `MediaFormatReader` obtains from a media resource decoded samples intended
for forward playback.

`MediaFormatReader::Update()` manages transitions between multiple states.
The key transitions are captured in the diagram below:
```{mermaid}

%% Work around https://github.com/mermaid-js/mermaid/issues/5785 %%
%%{init: {"flowchart": {"htmlLabels": false}}}%%

stateDiagram-v2

    DecodeError : Decode error
    EOSDrain : Drain on end of stream
    FatalDecodeError : Fatal decode error
    InternalSeek : Internal seek to random access point
    InternalSeekDemux : Demux to complete internal seek
    InternalSeekDecode : Decode to complete internal seek
    StreamChangeDrain : Drain of stream before stream change
    VideoSkip : Skip video demux to next key frame
    WaitingDrain : Drain when waiting for more data

    [*] --> Demux
    Demux --> Decode
    Demux --> WaitingDrain
    Demux --> StreamChangeDrain
    Demux --> EOSDrain
    Decode --> Demux
    Decode --> DecodeError
    Decode --> VideoSkip
    DecodeError --> FatalDecodeError
    DecodeError --> InternalSeek
    DecodeError --> VideoSkip
    WaitingDrain --> InternalSeek
    WaitingDrain --> DecodeError
    StreamChangeDrain --> Decode
    StreamChangeDrain --> InternalSeek
    StreamChangeDrain --> DecodeError
    InternalSeek --> InternalSeekDemux
    InternalSeekDemux --> InternalSeekDecode
    InternalSeekDemux --> VideoSkip
    InternalSeekDecode --> InternalSeekDemux
    InternalSeekDecode --> VideoSkip
    InternalSeekDecode --> Demux
    VideoSkip --> Demux
    VideoSkip --> DecodeError
    FatalDecodeError --> [*]
    EOSDrain --> [*]
```
