#include "Common.hlsli"

// **Blue Noise Pattern 8×8 (縦線ノイズ除去用)**
static const float BlueNoise8x8[64] = {
    0.515625f, 0.140625f, 0.890625f, 0.328125f, 0.484375f, 0.171875f, 0.921875f, 0.359375f,
    0.015625f, 0.765625f, 0.265625f, 0.703125f, 0.046875f, 0.796875f, 0.296875f, 0.734375f,
    0.640625f, 0.078125f, 0.828125f, 0.203125f, 0.671875f, 0.109375f, 0.859375f, 0.234375f,
    0.390625f, 0.953125f, 0.453125f, 0.578125f, 0.421875f, 0.984375f, 0.484375f, 0.609375f,
    0.546875f, 0.125000f, 0.859375f, 0.281250f, 0.515625f, 0.156250f, 0.890625f, 0.312500f,
    0.078125f, 0.734375f, 0.234375f, 0.656250f, 0.109375f, 0.765625f, 0.265625f, 0.687500f,
    0.703125f, 0.031250f, 0.796875f, 0.156250f, 0.734375f, 0.062500f, 0.828125f, 0.187500f,
    0.343750f, 0.906250f, 0.406250f, 0.531250f, 0.375000f, 0.937500f, 0.437500f, 0.562500f
};

// **改良されたシード生成関数（Blue Noise Pattern使用）**
uint GenerateBlueNoiseSeed(uint2 pixelCoord, uint frameIndex, uint sampleIndex)
{
    // Blue Noise Patternからベース値を取得
    uint2 noiseCoord = pixelCoord & 7; // 8×8パターンでタイル
    uint noiseIndex = noiseCoord.y * 8 + noiseCoord.x;
    float blueNoiseValue = BlueNoise8x8[noiseIndex];
    
    // Blue Noise値を整数に変換
    uint blueNoiseSeed = uint(blueNoiseValue * 4294967295.0f); // 32bit max
    
    // **最適化: PCGHash使用でパフォーマンス重視**
    uint seed = blueNoiseSeed;
    seed ^= PCGHash(pixelCoord.x * 73856093u);  // 大きな素数
    seed ^= PCGHash(pixelCoord.y * 19349663u);  // 大きな素数
    seed ^= PCGHash(frameIndex * 83492791u);    // フレーム変動用
    seed ^= PCGHash(sampleIndex * 51726139u);   // サンプル変動用
    
    return PCGHash(seed);
}

[shader("raygeneration")]
void RayGen()
{
    // デバッグモード：G-Buffer出力テスト
    bool rayGenDebugMode = false;
    if (rayGenDebugMode) {
        uint3 launchIndex = DispatchRaysIndex();
        float2 uv = float2(launchIndex.xy) / float2(DispatchRaysDimensions().xy);
        
        // **各バッファに識別可能な異なる色を出力**
        // u0: RenderTarget - グラデーション（赤→緑）
        RenderTarget[launchIndex.xy] = float4(uv.x, uv.y, 0.0f, 1.0f);
        
        // u1: AccumulationBuffer - 青色
        AccumulationBuffer[launchIndex.xy] = float4(0.0f, 0.0f, 1.0f, 1.0f);
        
        // u2: PrevFrameData - 黄色  
        PrevFrameData[launchIndex.xy] = float4(1.0f, 1.0f, 0.0f, 1.0f);
        
        // u3: AlbedoOutput - 赤色
        AlbedoOutput[launchIndex.xy] = float4(1.0f, 0.0f, 0.0f, 1.0f);
        
        // u4: NormalOutput - 緑色
        NormalOutput[launchIndex.xy] = float4(0.0f, 1.0f, 0.0f, 1.0f);
        
        // u5: DepthOutput - マゼンタ
        DepthOutput[launchIndex.xy] = float4(1.0f, 0.0f, 1.0f, 1.0f);
        return;
    }
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    
    // �A���`�G�C���A�V���O�p�}���`�T���v�����O
    float3 finalColor = float3(0, 0, 0);
    float3 finalAlbedo = float3(0, 0, 0);
    float3 finalNormal = float3(0, 0, 0);
    float finalDepth = 0.0f;
    uint finalMaterialType = 0;
    float finalRoughness = 0.0f;
    
    const int SAMPLES = 1; // パフォーマンス重視（デノイザーで品質補完）
    
    for (int sampleIndex = 0; sampleIndex < SAMPLES; sampleIndex++)
    {
        // **フレーム毎の乱数変動（正しいMonte Carlo積分）**
        uint frameIndex = uint(frameCount) % 64u; // 64パターンでサイクル
        uint seed = GenerateBlueNoiseSeed(launchIndex.xy, frameIndex, sampleIndex);
        
        // アンチエイリアシング用ジッター（時間的蓄積で滑らか化）
        float2 jitter = float2(RandomFloat(seed), RandomFloat(seed)) - 0.5f;
        float2 crd = float2(launchIndex.xy) + jitter;
        float2 dims = float2(launchDim.xy);
        
        // ���K�����W�ɕϊ� (-1 to 1)
        float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
        d.y = -d.y; // Y�����]
        
        // �V�����R�[�h�F
        float3 cameraPos = viewMatrix._m03_m13_m23; // �J�����ʒu�̂݃r���[�s�񂩂�擾

        // ���C�����v�Z�i���O�v�Z�ςݒl���g�p�j
        float3 rayDir = normalize(cameraForward +
                        d.x * cameraRight * tanHalfFov * aspectRatio +
                        d.y * cameraUp * tanHalfFov);
        
        // ���C�ݒ�
        RayDesc ray;
        ray.Origin = cameraPos;
        ray.Direction = rayDir;
        ray.TMin = 0.1f;
        ray.TMax = 10000.0f;
        
        // ���C�y�C���[�h�������i�S�t�B�[���h�𖾎��I�ɏ������j
        RayPayload payload;
        payload.color = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.depth = 0; // 4 bytes
        payload.seed = seed; // 4 bytes
        
        // G-Buffer�f�[�^�������i�S�t�B�[���h�𖾎��I�ɏ������j
        payload.albedo = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.normal = float3(0, 0, 1); // 4+4+4 = 12 bytes (�f�t�H���g�@��)
        payload.worldPos = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.hitDistance = 0.0f; // 4 bytes
        payload.materialType = 0; // 4 bytes
        payload.roughness = 0.0f; // 4 bytes
        payload.padding = 0; // 4 bytes
        
        // ���v: 72 bytes
        
        // ���C�g���[�V���O���s
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
        
        // 正常処理：TraceRayで取得したpayloadデータをそのまま使用
        
        // **ReSTIR DI処理（直接照明の改善）**
        // 注意：payload.colorには直接照明+間接照明が含まれているため、
        // ReSTIR DIでは間接照明のみを使用し、直接照明は置き換える
        float3 restirColor = payload.color;
        
        // ヒットした場合のみReSTIR DI処理（一時的に無効化してベース動作確認）
        if (false && payload.hitDistance > 0.0f && length(payload.worldPos) > 0.1f) {
            // 基本データ取得
            float3 worldPos = payload.worldPos;
            float3 normal = payload.normal;
            
            // ReSTIRバッファのインデックス計算
            uint2 screenDim = DispatchRaysDimensions().xy;
            uint reservoirIndex = GetReservoirIndex(launchIndex.xy, screenDim);
            
            // **段階1: 初期候補生成 (Initial Candidate Generation)**
            const int INITIAL_CANDIDATES = 4; // 白飛び防止のため候補数を減らす
            LightReservoir currentReservoir = InitLightReservoir();
            
            for (int i = 0; i < INITIAL_CANDIDATES; i++) {
                LightReservoir candidate = GenerateInitialLightSample(worldPos, normal, seed);
                currentReservoir = CombineLightReservoirs(currentReservoir, candidate, seed);
            }
            
            // **段階2: 時間的再利用 (Temporal Reuse)**
            // カメラ動き検出（ReSTIR専用）
            bool cameraMovedForReSTIR = (cameraMovedFlag != 0u);
            if (!cameraMovedForReSTIR && reservoirIndex < (screenDim.x * screenDim.y)) {
                // 前フレームのReservoirを取得
                LightReservoir prevReservoir = PreviousReservoirs[reservoirIndex];
                
                // 前フレームReservoirが有効で、現在のサーフェスと適合する場合に結合
                if (prevReservoir.valid && prevReservoir.weight > 0.0f) {
                    // 前フレームのライトサンプルでのTarget PDF再計算
                    LightInfo prevLight = GetLightByIndex(prevReservoir.lightIndex);
                    float prevTargetPdf = CalculateLightTargetPDF(worldPos, normal, prevLight, prevReservoir.samplePos);
                    
                    // バイアス軽減：Target PDFが正の値を持つ場合のみ再利用
                    if (prevTargetPdf > 0.0f) {
                        // MIS重み更新用の新しいReservoir作成
                        LightReservoir tempReservoir = InitLightReservoir();
                        UpdateLightReservoir(tempReservoir, prevReservoir.lightIndex, 
                                           prevReservoir.samplePos, prevReservoir.radiance,
                                           prevTargetPdf, prevReservoir.pdf, seed);
                        
                        // 現在のReservoirと時間的に結合
                        currentReservoir = CombineLightReservoirs(currentReservoir, tempReservoir, seed);
                    }
                }
            }
            
            // **段階3: 空間的再利用 (Spatial Reuse)**
            const int SPATIAL_NEIGHBORS = 2; // 白飛び防止のため近隣サンプル数を減らす
            const float SPATIAL_RADIUS = 8.0f; // 近隣サンプリング半径（ピクセル単位）
            
            for (int i = 0; i < SPATIAL_NEIGHBORS; i++) {
                // ランダムな近隣ピクセル選択
                float2 randomOffset = float2(RandomFloat(seed), RandomFloat(seed));
                randomOffset = (randomOffset - 0.5f) * 2.0f; // -1 to 1
                
                int2 neighborCoord = int2(launchIndex.xy) + int2(randomOffset * SPATIAL_RADIUS);
                
                // 画面境界チェック
                if (neighborCoord.x >= 0 && neighborCoord.x < int(screenDim.x) &&
                    neighborCoord.y >= 0 && neighborCoord.y < int(screenDim.y)) {
                    
                    uint neighborIndex = GetReservoirIndex(uint2(neighborCoord), screenDim);
                    if (neighborIndex < (screenDim.x * screenDim.y)) {
                        // 近隣ピクセルのReservoirを取得
                        LightReservoir neighborReservoir = CurrentReservoirs[neighborIndex];
                        
                        // 近隣Reservoirが有効で、サーフェス適合性チェック
                        if (neighborReservoir.valid && neighborReservoir.weight > 0.0f) {
                            // 近隣サンプルでのTarget PDF再計算
                            LightInfo neighborLight = GetLightByIndex(neighborReservoir.lightIndex);
                            float neighborTargetPdf = CalculateLightTargetPDF(worldPos, normal, neighborLight, neighborReservoir.samplePos);
                            
                            // バイアス軽減：適切なPDFを持つ場合のみ空間的再利用
                            if (neighborTargetPdf > 0.0f) {
                                // MIS重み更新用の新しいReservoir作成
                                LightReservoir tempReservoir = InitLightReservoir();
                                UpdateLightReservoir(tempReservoir, neighborReservoir.lightIndex,
                                                   neighborReservoir.samplePos, neighborReservoir.radiance,
                                                   neighborTargetPdf, neighborReservoir.pdf, seed);
                                
                                // 現在のReservoirと空間的に結合
                                currentReservoir = CombineLightReservoirs(currentReservoir, tempReservoir, seed);
                            }
                        }
                    }
                }
            }
            
            // **ReSTIR DI Reservoir正規化（RTXDI準拠）**
            // Target PDF再計算によるバイアス補正
            if (currentReservoir.valid && currentReservoir.weightSum > 0.0f) {
                // 選択されたサンプルでのTarget PDF再計算
                LightInfo selectedLight = GetLightByIndex(currentReservoir.lightIndex);
                float3 samplePos = currentReservoir.samplePos;
                float recalculatedTargetPdf = CalculateLightTargetPDF(worldPos, normal, selectedLight, samplePos);
                
                // RTXDI正規化: weightSum / target_pdf
                if (recalculatedTargetPdf > 0.0f) {
                    currentReservoir.weightSum = currentReservoir.weightSum / recalculatedTargetPdf;
                }
            }
            
            // **ReSTIR DIライトサンプルから最終照明計算**
            if (currentReservoir.valid && currentReservoir.weightSum > 0.0f) {
                // 選択されたライトサンプルで照明計算
                LightInfo selectedLight = GetLightByIndex(currentReservoir.lightIndex);
                float3 samplePos = currentReservoir.samplePos;
                
                // シャドウレイでオクルージョンチェック
                float3 lightDir = normalize(samplePos - worldPos);
                float lightDist = length(samplePos - worldPos);
                
                RayDesc shadowRay;
                shadowRay.Origin = worldPos + normal * 0.001f; // 自己遮蔽防止
                shadowRay.Direction = lightDir;
                shadowRay.TMin = 0.001f;
                shadowRay.TMax = lightDist - 0.001f;
                
                RayPayload shadowPayload;
                shadowPayload.color = float3(0, 0, 0);
                shadowPayload.depth = 0;
                shadowPayload.seed = seed;
                shadowPayload.albedo = float3(0, 0, 0);
                shadowPayload.normal = float3(0, 0, 1);
                shadowPayload.worldPos = float3(0, 0, 0);
                shadowPayload.hitDistance = 0.0f;
                shadowPayload.materialType = 0;
                shadowPayload.roughness = 0.0f;
                shadowPayload.padding = 0;
                
                TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 1, 1, 1, shadowRay, shadowPayload);
                
                // シャドウされていない場合のみ照明を適用
                if (shadowPayload.hitDistance <= 0.0f) {
                    // **NVIDIA RTXDI準拠のReSTIR DI照明計算**
                    float cosTheta = max(0.0f, dot(normal, lightDir));
                    float3 brdf = payload.albedo / 3.14159265f; // Lambertian BRDF
                    
                    // 距離減衰
                    float attenuation = 1.0f / (lightDist * lightDist);
                    
                    // RTXDI方式：weightSumがInverse PDFとして使用される
                    // Final shading: L = BRDF × Li × cosθ × (1/r²) × (1/PDF)
                    // currentReservoir.weightはweightSum/Mなので、実際はweightSumを使用
                    float invPdf = currentReservoir.weightSum; // Inverse PDF
                    float3 incomingRadiance = currentReservoir.radiance;
                    float3 directLight = brdf * incomingRadiance * cosTheta * attenuation * invPdf;
                    
                    // ReSTIR DI直接照明 + 従来の間接照明を合成
                    // payload.colorは間接照明のみ（debugMode=2で直接照明は無効化済み）
                    restirColor = directLight + payload.color;
                }
            }
            
            // 現在のReservoirを保存（次フレームの時間的再利用用）
            CurrentReservoirs[reservoirIndex] = currentReservoir;
        }
        
        // �T���v�����ʂ�~��
        finalColor += restirColor;
        
        // �v���C�}�����C��G-Buffer�f�[�^�̂ݒ~��
        if (sampleIndex == 0)  // ������ �ŏ��̃T���v���݂̂Ŕ�����ȑf�� ������
        {
            finalAlbedo += payload.albedo;
            finalNormal += payload.normal;
            finalDepth += payload.hitDistance;
            finalMaterialType = payload.materialType;
            finalRoughness += payload.roughness;
        }
    }
    
    // ���ω�
    finalColor /= float(SAMPLES);
    finalAlbedo /= float(SAMPLES);
    finalNormal /= float(SAMPLES);
    finalDepth /= float(SAMPLES);
    finalRoughness /= float(SAMPLES);
    
    // �@���̐��K��
    if (length(finalNormal) > 0.1f)
    {
        finalNormal = normalize(finalNormal);
    }
    else
    {
        finalNormal = float3(0, 0, 1); // �f�t�H���g�@��
    }
    
    // �|�X�g�v���Z�V���O
    float3 color = finalColor;
    
    // NaN�`�F�b�N
    if (any(isnan(color)) || any(isinf(color)))
    {
        color = float3(1, 0, 1); // マゼンタでエラー表示
    }
    
    // **物理的に正確なACESトーンマッピング**
    // ACES RRT/ODT近似（映画業界標準）
    float3 aces_input = color * 0.6f; // 入力スケーリング
    float3 a = aces_input * (aces_input + 0.0245786f) - 0.000090537f;
    float3 b = aces_input * (0.983729f * aces_input + 0.4329510f) + 0.238081f;
    color = a / b;
    
    // **色温度調整（Cornell Boxの白色点に最適化）**
    // 6500K色温度に調整（より自然な白色）
    float3x3 colorMatrix = float3x3(
        1.0478112f,  0.0228866f, -0.0501270f,
        -0.0295081f, 0.9904844f,  0.0150436f,
        -0.0092345f, 0.0150436f,  0.7521316f
    );
    color = mul(colorMatrix, color);
    
    // **適応的露出補正**
    float avgLuminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float targetLuminance = 0.18f; // 中間グレー基準
    float exposureAdjust = targetLuminance / max(avgLuminance, 0.001f);
    exposureAdjust = clamp(exposureAdjust, 0.5f, 2.0f); // 適度な範囲に制限
    color *= exposureAdjust;
    
    // **正確なsRGBガンマ補正**
    // リニア→sRGB変換（IEC 61966-2-1標準）
    float3 srgb;
    srgb.x = (color.x <= 0.0031308f) ? color.x * 12.92f : 1.055f * pow(color.x, 1.0f/2.4f) - 0.055f;
    srgb.y = (color.y <= 0.0031308f) ? color.y * 12.92f : 1.055f * pow(color.y, 1.0f/2.4f) - 0.055f;
    srgb.z = (color.z <= 0.0031308f) ? color.z * 12.92f : 1.055f * pow(color.z, 1.0f/2.4f) - 0.055f;
    color = srgb;
    
    // G-Buffer�̃K���}�␳��NaN�`�F�b�N
    if (any(isnan(finalAlbedo)) || any(isinf(finalAlbedo)))
        finalAlbedo = float3(0, 0, 0);
    if (any(isnan(finalNormal)) || any(isinf(finalNormal)))
        finalNormal = float3(0, 0, 1); // �f�t�H���g�@��
    if (isnan(finalDepth) || isinf(finalDepth))
        finalDepth = 0.0f;
    
    // ������ �C���F�[�x�l�𐳋K�����ĉ��� ������
    const float MAX_VIEW_DEPTH = 10.0f; // Cornell Boxスケールに最適化 // �J������Far�N���b�v�Ȃǂɍ��킹��
    float normalizedDepth = saturate(finalDepth / MAX_VIEW_DEPTH);

    // ���ʂ��o��
    // **時間的蓄積処理**
    // 法線を時間的蓄積用に正規化（0~1範囲）
    float3 normalizedNormal = finalNormal * 0.5f + 0.5f;
    float4 currentFrameData = float4(normalizedNormal, normalizedDepth);
    float4 prevAccumulation = AccumulationBuffer[launchIndex.xy];
    float frameCount = prevAccumulation.a;
    
    // **カメラ動き検出による強制リセット**
    bool forceReset = (cameraMovedFlag != 0u);
    
    // **初期フレーム検出**: frameCountが0または極小値の場合は初期フレーム
    bool isFirstFrame = (frameCount < 0.5f) || forceReset;
    
    float3 accumulatedColor;
    float newFrameCount;
    
    if (isFirstFrame) {
        // **初期フレーム処理**: 蓄積なしで現在色をそのまま使用
        accumulatedColor = color;
        newFrameCount = 1.0f;
    } else {
        // **通常フレーム処理**: 前フレームとの類似度チェック
        float4 prevFrameData = PrevFrameData[launchIndex.xy];
        
        // **重要: 0~1形式の法線データを-1~1に逆変換してから比較**
        float3 currentNormal = currentFrameData.rgb * 2.0f - 1.0f; // 0~1 → -1~1
        float3 prevNormal = prevFrameData.rgb * 2.0f - 1.0f;       // 0~1 → -1~1
        
        // 正しい法線ベクトルでの類似度計算
        float normalSimilarity = dot(currentNormal, prevNormal);
        float depthDiff = abs(currentFrameData.a - prevFrameData.a);
        
        // 深度差による重み付け（深度変化に敏感に）
        float depthSimilarity = exp(-depthDiff * 10.0f); // 指数関数で敏感な判定
        
        // 法線と深度の重み付け合成
        float similarity = normalSimilarity * 0.7f + depthSimilarity * 0.3f;
        
        // 正しい類似度判定（-1~1範囲に調整）
        // normalSimilarity: -1~1, depthSimilarity: 0~1
        // 組み合わせた similarity: 約-0.7~1.0 の範囲
        bool shouldAccumulate = (similarity > 0.9f);
        
        if (shouldAccumulate) {
            // **段階的減衰方式による蓄積継続**
            float3 prevColor = prevAccumulation.rgb;
            newFrameCount = frameCount + 1.0f;
            
            // 段階的減衰による適応的アルファ値
            float alpha;
            if (newFrameCount < 8.0f) {
                // 初期フレーム：速い収束
                alpha = 1.0f / newFrameCount;
            } else if (newFrameCount < 32.0f) {
                // 中期：バランス取れた収束
                alpha = 0.1f;
            } else if (newFrameCount < 64.0f) {
                // 後期：安定した微調整
                alpha = 0.05f;
            } else {
                // **制限後：固定重みで継続蓄積（リセットなし）**
                alpha = 0.02f; // 非常に緩やかな更新（2%）
                newFrameCount = 64.0f; // フレーム数は64で固定
            }
            
            accumulatedColor = lerp(prevColor, color, alpha);
        } else {
            // 蓄積リセット（類似度が低い場合のみ）
            accumulatedColor = color;
            newFrameCount = 1.0f;
        }
    }
    
    // 結果を出力
    RenderTarget[launchIndex.xy] = float4(accumulatedColor, 1.0f);
    AccumulationBuffer[launchIndex.xy] = float4(accumulatedColor, newFrameCount);
    PrevFrameData[launchIndex.xy] = currentFrameData;
    AlbedoOutput[launchIndex.xy] = float4(finalAlbedo, finalRoughness);
    NormalOutput[launchIndex.xy] = float4(finalNormal, 1.0f);
    
    DepthOutput[launchIndex.xy] = float4(normalizedDepth, normalizedDepth, normalizedDepth, 1.0f);
}