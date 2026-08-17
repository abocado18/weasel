


struct VsInput
{
    float4 position : POSITION;
};

struct VsOutput
{
    float4 position : SV_POSITION;

};

struct PsOutput
{
    float4 color : SV_TARGET;
};


VsOutput vertexMain(VsInput input)
{

VsOutput output;



output.position = input.position;

    return output;

}

PsOutput pixelMain(VsOutput input)
{
    PsOutput output;
    output.color = float4(1.0, 0.0, 0.0, 1.0);
    return output;
}
