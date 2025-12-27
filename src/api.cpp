// raw data is already in evaluation (NTT) form
// so we no longer need pre-computed NTT tables
// only reconstruct polynomial structure with the pre-computed NTT coefficients
#include "api.h"
#include <zephyr/sys/printk.h>

#include "keys.h" // Public key raw data
#include "openfhe.h"

using namespace lbcrypto;

void test_lib()
{
	printk("--- OpenFHE MCU Encryption (Patched 32-bit) ---\n");

	try
	{
		// 1. Generate Context
		printk("1. Generating Context...\n");
		CCParams<CryptoContextCKKSRNS> params;
		params.SetRingDim(2048);
		params.SetMultiplicativeDepth(0);
		params.SetFirstModSize(28);
		params.SetScalingModSize(20);
		params.SetSecurityLevel(HEStd_NotSet);
		params.SetKeySwitchTechnique(BV);
		params.SetScalingTechnique(FIXEDMANUAL);

		CryptoContext<DCRTPoly> cc = GenCryptoContext(params);
		cc->Enable(PKE);

		auto cryptoParams = std::dynamic_pointer_cast<CryptoParametersCKKSRNS>(
				cc->GetCryptoParameters());
		auto elementParams = cryptoParams->GetElementParams();
		auto paramsVec = elementParams->GetParams();

		uint32_t ringDim = elementParams->GetRingDimension();
		uint32_t numTowers = paramsVec.size();

		printk("   [OK] Context Ready. N=%u, Towers=%u\n", ringDim, numTowers);

		// 2. Reconstruct public key from flash
		printk("2. Reconstructing public key...\n");
		const uint32_t *rawInts = reinterpret_cast<const uint32_t *>(
				PublicKeyRawBuffer_raw + 8); // Skip 8-byte cereal size header

		std::vector<DCRTPoly> polyVec;
		size_t globalIdx = 0;

		for (int p = 0; p < 2; p++)
		{
			DCRTPoly poly(elementParams, Format::EVALUATION, true);

			for (size_t t = 0; t < numTowers; t++)
			{
				NativeVector towerValues(ringDim, paramsVec[t]->GetModulus());
				for (size_t i = 0; i < ringDim; i++)
				{
					towerValues[i] = NativeInteger(rawInts[globalIdx++]);
				}

				NativePoly towerPoly(paramsVec[t], Format::EVALUATION);
				towerPoly.SetValues(std::move(towerValues), Format::EVALUATION);
				poly.SetElementAtIndex(t, std::move(towerPoly));
			}
			polyVec.push_back(std::move(poly));
		}

		PublicKey<DCRTPoly> pk = std::make_shared<PublicKeyImpl<DCRTPoly>>(cc);
		pk->SetPublicElements(polyVec);
		polyVec.clear();
		polyVec.shrink_to_fit(); // Release memory immediately
		printk("   [OK] Public Key Reconstructed.\n");

		// 3. Encode and Encrypt
		printk("3. Encrypting...\n");
		std::vector<double> sensorData = {1.0, 2.0, 3.0};
		Plaintext ptxt = cc->MakeCKKSPackedPlaintext(sensorData);
		auto ciphertext = cc->Encrypt(pk, ptxt);
		printk("   [OK] Encryption complete. Level: %u\n", ciphertext->GetLevel());

		// 4. Export Ciphertext as hex
		printk("\n=== CIPHERTEXT BEGIN ===\n");
		const std::vector<DCRTPoly> &ctElements = ciphertext->GetElements();

		printk("ELEMENTS:%u\n", (unsigned)ctElements.size());
		printk("TOWERS:%u\n", numTowers);
		printk("RINGDIM:%u\n", ringDim);
		printk("LEVEL:%u\n", ciphertext->GetLevel());
		printk("SCALINGFACTOR:%llu\n",
					 (unsigned long long)ciphertext->GetScalingFactor());
		printk("DATA:\n");

		for (const auto &poly : ctElements)
		{
			const auto &towers = poly.GetAllElements();
			for (const auto &tower : towers)
			{
				const auto &vec = tower.GetValues();
				for (size_t i = 0; i < vec.GetLength(); i++)
				{
					printk("%08x", vec[i].ConvertToInt());
					if ((i + 1) % 8 == 0)
						printk("\n");
				}
			}
		}
		printk("\n=== CIPHERTEXT END ===\n");
	}
	catch (const std::exception &e)
	{
		printk("EXCEPTION: %s\n", e.what());
	}
}
