#include "GunMountMesh.h"

GunMountMesh* GunMountMesh::mesh = NULL;

GunMountMesh* GunMountMesh::getInstance(IDirect3DDevice9* device)
{
	if (mesh == NULL)
	{
		mesh = new GunMountMesh(device);
	}

	return mesh;
}


GunMountMesh::GunMountMesh(IDirect3DDevice9* device)
{
	initialize(device);
}


GunMountMesh::~GunMountMesh()
{
}

void GunMountMesh::render(IDirect3DDevice9* device)
{
	device->SetStreamSource(0, vertexBuffer, 0, sizeof(Vertex));
	device->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2);
	device->SetIndices(indexBuffer);
	device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vertexCount, 0, indexCount / 3);
}


void GunMountMesh::initialize(IDirect3DDevice9* device)
{
	Vertex* verts;
	unsigned long* inds;

	loadMesh("models/gunmount.ese");

	device->CreateVertexBuffer(sizeof(Vertex) * vertexCount, D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2,
		D3DPOOL_MANAGED, &vertexBuffer, NULL);


	vertexBuffer->Lock(0, sizeof(Vertex) * vertexCount, (void**) &verts, NULL);

	memcpy(verts, vertices, sizeof(Vertex) * vertexCount);

	vertexBuffer->Unlock();

	device->CreateIndexBuffer(sizeof(unsigned long) * indexCount, D3DUSAGE_WRITEONLY, D3DFMT_INDEX32,
		D3DPOOL_MANAGED, &indexBuffer, NULL);

	indexBuffer->Lock(0, sizeof(unsigned long) * indexCount, (void**) &inds, NULL);

	memcpy(inds, indices, sizeof(unsigned long) * indexCount);

	indexBuffer->Unlock();

	return;
}
