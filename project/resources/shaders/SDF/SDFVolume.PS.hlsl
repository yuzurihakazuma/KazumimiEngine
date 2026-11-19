// SDFボリューム（レイマーチング）用 ピクセルシェーダー
// Texture3D<float> の符号付き距離をスフィアトレーシングして面を再構成する。
//   ・エロージョン : 距離に定数を足す＝表面が法線方向に痩せる/太る（溶け・出現）
//   ・モーフィング : 2つの距離場を lerp ＝ 形Aから形Bへ連続変形（メッシュでは不可能）
//   ・カラー       : .sdfcol（最寄り表面色の焼き込み）をヒット点でサンプル
//   ・SV_Depth にヒット点の深度を書くので、通常の3Dモデルと正しく前後関係が出る
//   ・実体と同じ見た目（meshLook）: 色をカラーボリューム（64^3 でぼやける）ではなく、実体のメッシュと同じテクスチャ
//     （球の展開の UV を当たった点から求める）で付け、光も Object3d.PS と同じ式・同じライトで当てる。
//     SDF の物から実体のメッシュへ交代する時に、模様・明るさ・つやが変わらない（産卵の卵）

struct VolumeCB
{
    float4x4 viewProj;
    float3 rayBoxMin; float pad0;  // レイ/プロキシ箱（モーフ時は A∪B）
    float3 rayBoxMax; float pad1;
    float3 boxMinA; float pad2;    // ボリュームAのワールドAABB
    float3 boxMaxA; float pad3;
    float3 boxMinB; float pad4;    // ボリュームB（モーフ先）のワールドAABB
    float3 boxMaxB; float pad5;
    float3 cameraPos; float distScale;
    float4 baseColor;
    float3 lightDir; float erode;
    float useColorTexA; float useColorTexB; float useMorph; float morphT;
    float meltStyle; float meltNoiseFreq; float meltBand; float meltPad; // 溶け方（0=均一/1=段ボール焼け）
    float meshLook; float shininess; float depthBias; float uvHalfHeight; // 実体と同じ見た目・つや・深度を手前へずらす量(m)・UV の高さの半分
    float3 center; float pixelAngle;                                      // 物の中心（ワールド）・1 画素の角度(rad)
};
ConstantBuffer<VolumeCB> gVolume : register(b0);

// ライト（Object3d.hlsli と同じ並び。Obj3dCommon の定数バッファをそのまま挿す）
struct DirectionalLight { float4 color; float3 direction; float intensity; };
struct DirectionalLightData { DirectionalLight lights[4]; int activeCount; float3 padding; };
struct PointLight { float4 color; float3 position; float intensity; float radius; float decay; };
struct SpotLight { float4 color; float3 position; float intensity; float3 direction; float distance; float decay; float cosAngle; float cosFalloffStart; };
ConstantBuffer<DirectionalLightData> gDirectionalLightData : register(b1);
ConstantBuffer<PointLight> gPointLight : register(b2);
ConstantBuffer<SpotLight> gSpotLight : register(b3);
Texture2D<float4> gSurfaceTex : register(t4); // 実体のメッシュと同じテクスチャ（meshLook の時だけ読む）

Texture3D<float>  gSDFA      : register(t0); // 距離ボリュームA
Texture3D<float4> gColorVolA : register(t1); // カラーボリュームA
Texture3D<float>  gSDFB      : register(t2); // 距離ボリュームB（モーフ先）
Texture3D<float4> gColorVolB : register(t3); // カラーボリュームB
SamplerState gSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_TARGET0; // シーンの色キャンバス
    float4 mask  : SV_TARGET1; // エフェクトマスク（ブルーム対象外なので0）
    float  depth : SV_Depth;   // ヒット点の正確な深度
};

// あるボリュームの距離をワールド座標でサンプル（箱の外は境界までの距離を足して安全に外挿）
float SampleVol(Texture3D<float> vol, float3 bmin, float3 bmax, float3 wp)
{
    float3 uv = (wp - bmin) / (bmax - bmin);
    float3 uvc = clamp(uv, 0.0f, 1.0f);
    float d = vol.SampleLevel(gSampler, uvc, 0) * gVolume.distScale;
    float3 cp = bmin + uvc * (bmax - bmin); // 箱内の最近点
    return d + length(wp - cp);
}

// --- 3D値ノイズ（ハッシュ＋トリリニア補間。段ボール焼けの「溶けムラ」用）---
float Hash3(float3 p)
{
    p = frac(p * 0.3183099f + float3(0.1f, 0.2f, 0.3f));
    p *= 17.0f;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float ValueNoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f); // スムースステップ補間
    float n000 = Hash3(i + float3(0, 0, 0)), n100 = Hash3(i + float3(1, 0, 0));
    float n010 = Hash3(i + float3(0, 1, 0)), n110 = Hash3(i + float3(1, 1, 0));
    float n001 = Hash3(i + float3(0, 0, 1)), n101 = Hash3(i + float3(1, 0, 1));
    float n011 = Hash3(i + float3(0, 1, 1)), n111 = Hash3(i + float3(1, 1, 1));
    return lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y),
                lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
}

// その点の実効エロージョン量。
//   段ボール焼けスタイルではワールド座標ノイズで不均一化：溶けやすい所（ノイズ大）は
//   先に穴が開き、溶けにくい所は繊維が残る＝熱した段ボールの崩れ方になる
float ErodeAt(float3 wp)
{
    float e = gVolume.erode;
    if (gVolume.meltStyle > 0.5f)
    {
        float n = ValueNoise(wp * gVolume.meltNoiseFreq);
        e *= lerp(0.45f, 1.65f, n);
    }
    return e;
}

// 合成距離場：モーフ（AとBのlerp）＋エロージョン
float MapDist(float3 wp)
{
    float d = SampleVol(gSDFA, gVolume.boxMinA, gVolume.boxMaxA, wp);
    if (gVolume.useMorph > 0.5f)
    {
        float dB = SampleVol(gSDFB, gVolume.boxMinB, gVolume.boxMaxB, wp);
        d = lerp(d, dB, gVolume.morphT);
    }
    return d + ErodeAt(wp);
}

PSOutput main(PSInput input)
{
    // レイ：カメラ → このピクセルのボックス表面点
    float3 ro = gVolume.cameraPos;
    float3 rd = normalize(input.worldPos - ro);

    // スラブ法でレイとAABB（レイ箱）の交差区間 [t0, t1] を求める
    float3 invD = 1.0f / rd;
    float3 tA = (gVolume.rayBoxMin - ro) * invD;
    float3 tB = (gVolume.rayBoxMax - ro) * invD;
    float3 tMin3 = min(tA, tB);
    float3 tMax3 = max(tA, tB);
    float t0 = max(max(tMin3.x, tMin3.y), tMin3.z);
    float t1 = min(min(tMax3.x, tMax3.y), tMax3.z);
    if (t1 <= max(t0, 0.0f)) { discard; }

    // スフィアトレーシング
    float boxDiag = length(gVolume.rayBoxMax - gVolume.rayBoxMin);
    float eps = boxDiag * 0.0015f; // ヒット判定のしきい値（箱の大きさに比例）
    float t = max(t0, 0.0f) + eps;
    float3 p = ro + rd * t;
    bool hit = false;

    [loop]
    for (int i = 0; i < 128; ++i)
    {
        p = ro + rd * t;
        float d = MapDist(p);
        if (d < eps) { hit = true; break; }
        t += max(d, eps); // 最低でも eps は進める（無限ループ防止）
        if (t > t1) { break; }
    }
    if (!hit) { discard; }

    // 法線 = 合成距離場の勾配（中央差分）。モーフ中間形状でも正しい向きが出る
    float h = boxDiag * 0.01f;
    float3 n = normalize(float3(
        MapDist(p + float3(h, 0, 0)) - MapDist(p - float3(h, 0, 0)),
        MapDist(p + float3(0, h, 0)) - MapDist(p - float3(0, h, 0)),
        MapDist(p + float3(0, 0, h)) - MapDist(p - float3(0, 0, h))));

    // 色：各ボリュームの焼き込み色（無ければ単色）を morphT でブレンド
    float3 uvA = clamp((p - gVolume.boxMinA) / (gVolume.boxMaxA - gVolume.boxMinA), 0.0f, 1.0f);
    float3 albedoA = lerp(gVolume.baseColor.rgb,
                          gColorVolA.SampleLevel(gSampler, uvA, 0).rgb, gVolume.useColorTexA);
    float3 albedo = albedoA;
    if (gVolume.useMorph > 0.5f)
    {
        float3 uvB = clamp((p - gVolume.boxMinB) / (gVolume.boxMaxB - gVolume.boxMinB), 0.0f, 1.0f);
        float3 albedoB = lerp(gVolume.baseColor.rgb,
                              gColorVolB.SampleLevel(gSampler, uvB, 0).rgb, gVolume.useColorTexB);
        albedo = lerp(albedoA, albedoB, gVolume.morphT);
    }

    // ハーフランバート＋環境光（クラフト風のやわらかい陰影）
    float3 l = normalize(-gVolume.lightDir);
    float ndl = saturate(dot(n, l) * 0.5f + 0.5f);
    float3 rgb = albedo * (0.25f + 0.75f * ndl);

    // --- 実体と同じ見た目：同じテクスチャ・同じ光の式（Object3d.PS）---
    if (gVolume.meshLook > 0.5f)
    {
        // 痩せている間（育つ途中）は、元の表面の点まで戻して UV を求める（模様が縮んだり流れたりしない）。
        //   距離の値は正規化されていることがあるので、数回くり返して寄せる
        float3 q = p;
        if (abs(gVolume.erode) > 1e-5f)
        {
            for (int k = 0; k < 4; ++k) { q -= n * SampleVol(gSDFA, gVolume.boxMinA, gVolume.boxMaxA, q); }
        }
        // 球の展開の UV（卵のモデルと同じ式）。メッシュは読み込み時に X を反転しているので、ここでも X を返す
        float3 local = (q - gVolume.center) / gVolume.distScale;
        float u = frac(atan2(local.z, -local.x) / 6.2831853f + 1.0f);
        float v = 1.0f - acos(clamp(-local.y / gVolume.uvHalfHeight, -1.0f, 1.0f)) / 3.14159265f;
        // ミップの段：1 画素の大きさ ÷ 1 テクセルの大きさ（UV のつなぎ目で微分が暴れないように、式で決める）
        float texW, texH, mips;
        gSurfaceTex.GetDimensions(0, texW, texH, mips);
        float texelWorld = 2.2f * gVolume.distScale / max(texW, 1.0f);
        float lod = clamp(log2(max(t * gVolume.pixelAngle / texelWorld, 1e-4f)), 0.0f, mips - 1.0f);
        float3 tex = gSurfaceTex.SampleLevel(gSampler, float2(u, v), lod).rgb;

        float3 toEye = normalize(gVolume.cameraPos - p);
        float3 diffuse = float3(0.0f, 0.0f, 0.0f);
        float3 specular = float3(0.0f, 0.0f, 0.0f);
        for (int li = 0; li < gDirectionalLightData.activeCount; ++li)
        {
            DirectionalLight light = gDirectionalLightData.lights[li];
            float c = pow(dot(n, -light.direction) * 0.5f + 0.5f, 2.0f);
            diffuse += tex * light.color.rgb * c * light.intensity;
            float3 halfVector = normalize(-light.direction + toEye);
            specular += light.color.rgb * light.intensity * pow(saturate(dot(n, halfVector)), gVolume.shininess);
        }
        {   // 点光源
            float3 dir = normalize(p - gPointLight.position);
            float c = pow(dot(n, -dir) * 0.5f + 0.5f, 2.0f);
            float factor = pow(saturate(-length(gPointLight.position - p) / gPointLight.radius + 1.0f), gPointLight.decay);
            diffuse += tex * gPointLight.color.rgb * c * gPointLight.intensity * factor;
            float3 halfVector = normalize(-dir + toEye);
            specular += gPointLight.color.rgb * gPointLight.intensity * pow(saturate(dot(n, halfVector)), gVolume.shininess) * factor;
        }
        {   // スポットライト
            float3 dir = normalize(p - gSpotLight.position);
            float falloff = saturate((dot(dir, gSpotLight.direction) - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
            float attenuation = pow(saturate(-length(gSpotLight.position - p) / gSpotLight.distance + 1.0f), gSpotLight.decay);
            float c = pow(dot(n, -dir) * 0.5f + 0.5f, 2.0f);
            diffuse += tex * gSpotLight.color.rgb * c * gSpotLight.intensity * attenuation * falloff;
            float3 halfVector = normalize(-dir + toEye);
            specular += gSpotLight.color.rgb * gSpotLight.intensity * pow(saturate(dot(n, halfVector)), gVolume.shininess) * attenuation * falloff;
        }
        rgb = tex * 0.15f + diffuse + specular;
    }

    // --- 段ボール焼け：溶け際の焦げ＋赤熱 ---
    //   ここに到達した表面は「元の深さ ≒ 局所エロージョン量」の溶け前線。
    //   局所エロージョンが基準量(meltBand)に近い所ほど今まさに焼け落ちる寸前なので、
    //   焦げ茶 → 黒 → オレンジ発光の順で色付けする（穴の縁がふちどりで赤熱して見える）
    if (gVolume.meltStyle > 0.5f)
    {
        float charT = saturate(ErodeAt(p) / max(gVolume.meltBand, 1e-3f));
        float3 charColor = float3(0.13f, 0.08f, 0.05f); // 焦げ（こげ茶〜黒）
        rgb = lerp(rgb, charColor, smoothstep(0.25f, 0.85f, charT) * 0.9f);
        float ember = smoothstep(0.55f, 0.95f, charT);  // 消える寸前の縁だけ
        rgb += float3(1.0f, 0.35f, 0.06f) * ember * 1.6f; // 赤熱（ライティング後に加算＝自発光）
    }

    PSOutput output;
    output.color = float4(rgb, gVolume.baseColor.a);
    output.mask  = float4(0.0f, 0.0f, 0.0f, 0.0f);

    // ヒット点のクリップ空間深度（depthBias：同じ形の実体と重ねる時に、ほんの少しカメラ側へずらして面の取り合いを防ぐ）
    float4 clip = mul(float4(p - rd * gVolume.depthBias, 1.0f), gVolume.viewProj);
    output.depth = clip.z / clip.w;
    return output;
}
